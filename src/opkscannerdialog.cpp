/***************************************************************************
 *   Copyright (C) 2006 by Massimiliano Torromeo   *
 *   massimiliano.torromeo@gmail.com   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "opkscannerdialog.h"
#include "utilities.h"
#include "menu.h"
#include "powermanager.h"
#include "debug.h"
#include "linkapp.h"
#include <dirent.h>
#include <libopk.h>
#include "filelister.h"
#include <algorithm>

extern const char *CARD_ROOT;

bool any_platform = false;

OPKScannerDialog::OPKScannerDialog(GMenu2X *gmenu2x, const string &title, const string &description, const string &icon, const string &backdrop):
TextDialog(gmenu2x, title, description, icon, backdrop) {}

void OPKScannerDialog::opkInstall(const string &path) {
	string pkgname = base_name(path, true);

	struct OPK *opk = opk_open(path.c_str());

	if (!opk) {
		text.push_back(path + ": " + _("Unable to open OPK"));
		lineWidth = drawText(&text, firstCol, -1, rowsPerPage);

		ERROR("%s: Unable to open OPK", path.c_str());
		return;
	}

	while (true) {
		const char *name;
		int ret = opk_open_metadata(opk, &name);
		if (ret < 0) {
			text.push_back(path + ": " + _("Error loading meta-data"));
			lineWidth = drawText(&text, firstCol, -1, rowsPerPage);
			ERROR("Error loading meta-data");
			goto close;
		} else if (ret == 0) {
			goto close;
		}

		/* Strip .desktop */
		string linkname(name), platform;
		string::size_type pos = linkname.rfind('.');
		linkname = linkname.substr(0, pos);

		pos = linkname.rfind('.');
		platform = linkname.substr(pos + 1);

		string linkpath = linkname.substr(0, pos);
		linkpath = pkgname + "." + linkname + ".lnk";

		if (!(any_platform || platform == gmenu2x->platform->opk || platform == "all")) {
			text.push_back(" - " + linkname + ": " + _("Unsupported platform"));
			lineWidth = drawText(&text, firstCol, -1, rowsPerPage);

			ERROR("%s: Unsupported platform '%s'", pkgname.c_str(), platform.c_str());
			continue;
		} else {
			text.push_back(" + " + linkname + ": " + _("OK"));
			lineWidth = drawText(&text, firstCol, -1, rowsPerPage);
		}

		const char *key, *val;
		size_t lkey, lval;

		string
			title = "",
			params = "",
			description = "",
			manual = "",
			selectordir = "",
			selectorfilter = "",
			aliasfile = "",
			scaling = "",
			icon = "",
			section = "applications";
		bool terminal = false;

		while (ret = opk_read_pair(opk, &key, &lkey, &val, &lval)) {
			if (ret < 0) {
				text.push_back(path + ": " + _("Error loading meta-data"));
				lineWidth = drawText(&text, firstCol, -1, rowsPerPage);
				ERROR("Error loading meta-data");
				goto close;
			} else if (ret == 0) {
				goto close;
			}

			char buf[lval + 1];
			sprintf(buf, "%.*s", lval, val);

			if (!strncmp(key, "Name", lkey)) {
				title = buf;
			} else
			if (!strncmp(key, "Exec", lkey)) {
				params = buf;
			} else
			if (!strncmp(key, "Comment", lkey)) {
				description = buf;
			} else
			if (!strncmp(key, "Terminal", lkey)) {
				terminal = !strcmp(buf, "true");
			} else
			if (!strncmp(key, "X-OD-Manual", lkey)) {
				manual = buf;
			} else
			if (!strncmp(key, "X-OD-Selector", lkey)) {
				selectordir = buf;
			} else
			if (!strncmp(key, "X-OD-Scaling", lkey)) {
				scaling = buf;
			} else
			if (!strncmp(key, "X-OD-Filter", lkey)) {
				selectorfilter = buf;
			} else
			if (!strncmp(key, "X-OD-Alias", lkey)) {
				aliasfile = buf;
			} else
			if (!strncmp(key, "Categories", lkey)) {
				section = buf;
				pos = section.find(';');
				if (pos != section.npos) {
					section = section.substr(0, pos);
				}
			} else
			if (!strncmp(key, "Icon", lkey)) {
				icon = path + "#" + (string)buf + ".png";
			}
		}

		gmenu2x->menu->addSection(section);

		linkpath = homePath + "/sections/" + section + "/" + linkpath;

		LinkApp *link = new LinkApp(gmenu2x, linkpath.c_str());

		if (!path.empty()) link->setExec(path);
		if (!params.empty()) link->setParams(params);
		if (!title.empty()) link->setTitle(title);
		if (!description.empty()) link->setDescription(description);
		if (!manual.empty()) link->setManual(manual);
		if (((char)params.find("\%f") >= 0) && link->getSelectorDir().empty()) link->setSelectorDir(gmenu2x->confStr["homePath"]);
		if (!selectordir.empty() && link->getSelectorDir().empty()) link->setSelectorDir(selectordir);
		if (!selectorfilter.empty()) link->setSelectorFilter(selectorfilter);
		if (!aliasfile.empty()) link->setAliasFile(aliasfile);
		if (!icon.empty()) link->setIcon(icon);
		if (!scaling.empty()) link->setScaleMode(atoi(scaling.c_str()));
		link->setTerminal(terminal);

		link->save();
	}

	close:
	opk_close(opk);
}

void OPKScannerDialog::opkScan(string opkdir) {
	FileLister fl;
	fl.showDirectories = false;
	fl.showFiles = true;
	fl.setFilter(".opk");
	fl.browse(opkdir);

	for (uint32_t i = 0; i < fl.size(); i++) {
		text.push_back(_F("Installing %s", fl.getPath(i).c_str()));
		lineWidth = drawText(&text, firstCol, -1, rowsPerPage);
		opkInstall(fl.getPath(i));
	}
}

void OPKScannerDialog::exec(bool _any_platform) {
	any_platform = _any_platform;
	rowsPerPage = gmenu2x->listRect.h / gmenu2x->font->height();

	gmenu2x->powerManager->clearTimer();

	if (gmenu2x->sc[this->icon] == NULL) {
		this->icon = "skin:icons/terminal.png";
	}

	buttons.push_back({"skin:imgs/manual.png", _("Running.. Please wait..")});

	drawDialog(gmenu2x->s);

	gmenu2x->s->flip();

	if (!opkpath.empty()) {
		text.push_back(_F("Installing %s", base_name(opkpath).c_str()));
		lineWidth = drawText(&text, firstCol, -1, rowsPerPage);
		opkInstall(opkpath);
	} else {
		vector<string> paths;
		paths.clear();

		FileLister fl;
		fl.showDirectories = true;
		fl.showFiles = false;
		fl.allowDirUp = false;

		fl.browse(CARD_ROOT);
		for (uint32_t j = 0; j < fl.size(); j++) {
			if (find(paths.begin(), paths.end(), fl.getPath(j)) == paths.end()) {
				paths.push_back(fl.getPath(j));
			}
		}

		if (gmenu2x->confStr["homePath"] != CARD_ROOT) {
			fl.browse(gmenu2x->confStr["homePath"]);
			for (uint32_t j = 0; j < fl.size(); j++) {
				if (find(paths.begin(), paths.end(), fl.getPath(j)) == paths.end()) {
					paths.push_back(fl.getPath(j));
				}
			}
		}

		fl.browse("/media");
		for (uint32_t i = 0; i < fl.size(); i++) {
			FileLister flsub;
			flsub.showDirectories = true;
			flsub.showFiles = false;
			flsub.allowDirUp = false;

			flsub.browse(fl.getPath(i));
			for (uint32_t j = 0; j < flsub.size(); j++) {
				if (find(paths.begin(), paths.end(), flsub.getPath(j)) == paths.end()) {
					paths.push_back(flsub.getPath(j));
				}
			}
		}

		for (uint32_t i = 0; i < paths.size(); i++) {
			text.push_back(_F("Scanning %s", paths[i].c_str()));
			lineWidth = drawText(&text, firstCol, -1, rowsPerPage);
			opkScan(paths[i]);
		}
	}

	system("sync &");

	text.push_back("----");
	text.push_back(_("Done"));

	if (text.size() >= rowsPerPage) {
		firstRow = text.size() - rowsPerPage;
	}

	buttons.clear();

	TextDialog::exec();
}
