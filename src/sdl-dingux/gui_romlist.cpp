/*
 * FinalBurn Alpha for Dingux/OpenDingux
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <set>
#include <algorithm>
#include <unordered_set>
#include <fstream>
#include <sstream>
#include <streambuf>
#if defined(OD_LEPUS)
// Needed in lepus to include stat definition
#include <sys/stat.h>
#endif

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>

#include "burner.h"
#include "gui_main.h"
#include "gui_romlist.h"

FILTER_DESC filter[] =
{
	{ 0, "All" },
	{ 1, "Missing" },
	{ 2, "Available" },
	{ 3, "Playable" },
	{ 4, "Favorites" }
};

FILTER_DESC hardwares[] =
{
	{ (0xffffffff), "All" },
	{ HARDWARE_PREFIX_SNK, "Neo Geo" },
	{ HARDWARE_PREFIX_CAPCOM, "CPS-1" },
	{ HARDWARE_PREFIX_CPS2, "CPS-2" },
	{ HARDWARE_PREFIX_CPS3, "CPS-3" },
	{ HARDWARE_PREFIX_IGS_PGM, "PGM" },
	{ HARDWARE_PREFIX_PSIKYO, "Psikyo" },
	{ HARDWARE_PREFIX_CAVE, "Cave" },
	{ HARDWARE_PREFIX_SEGA, "Sega" },
	{ HARDWARE_PREFIX_CAPCOM_MISC, "Capcom (Other)" },
	{ HARDWARE_PREFIX_DATAEAST, "Data East" },
	{ HARDWARE_PREFIX_GALAXIAN, "Galaxian" },
	{ HARDWARE_PREFIX_IREM, "Irem" },
	{ HARDWARE_PREFIX_KANEKO, "Kaneko" },
	{ HARDWARE_PREFIX_KONAMI, "Konami" },
	{ HARDWARE_PREFIX_MISC_PRE90S, "Misc (pre 90s)" },
	{ HARDWARE_PREFIX_MISC_POST90S, "Misc (post 90s)" },
	{ HARDWARE_PREFIX_PACMAN, "Pacman" },
	{ HARDWARE_PREFIX_SETA, "Seta" },
	{ HARDWARE_PREFIX_TAITO, "Taito" },
	{ HARDWARE_PREFIX_TECHNOS, "Technos" },
	{ HARDWARE_PREFIX_TOAPLAN, "Toaplan" },
	{ HARDWARE_PREFIX_MIDWAY, "Midway" },
	//{ HARDWARE_PREFIX_NINTENDO_SNES, "SNES" },
	//{ HARDWARE_PREFIX_SEGA_MEGADRIVE, "Megadrive" },
	//{ HARDWARE_PREFIX_PCENGINE, "PC-Engine" },
	//{ HARDWARE_PREFIX_SEGA_MASTER_SYSTEM, "Master System" }
};

FILTER_DESC genres[] =
{
	{ (0xffffffff), "All" },
	{ GBF_HORSHOOT, "Shooter - Horizontal" },
	{ GBF_VERSHOOT, "Shooter - Vertical" },
	{ GBF_SHOOT, "Shooter - Other" },
	{ GBF_SCRFIGHT, "Fighter - Scrolling" },
	{ GBF_VSFIGHT, "Fighter - Versus" },
	{ GBF_PLATFORM, "Platform" },
	{ GBF_RACING, "Racing" },
	{ GBF_SPORTSMISC, "Sports - Misc" },
	{ GBF_SPORTSFOOTBALL, "Sports - Football" },
	{ GBF_MAZE, "Maze" },
	{ GBF_BALLPADDLE, "Ball & Paddle" },
	{ GBF_PUZZLE, "Puzzle" },
	{ GBF_MINIGAMES, "Mini-Games" },
	{ GBF_BREAKOUT, "Breakout" },
	{ GBF_MISC, "Misc" },
	{ GBF_PINBALL, "Pinball" },
	{ GBF_CASINO, "Casino" },
	{ GBF_MAHJONG, "Mahjong" },
	{ GBF_QUIZ, "Quiz" },
	{ GBF_ACTION, "Action" },
	{ GBF_RUNGUN, "RunGun" },
	{ GBF_STRATEGY, "Strategy" },
	{ GBF_VECTOR, "Vector" },
	{ GBF_BIOS, "BIOS" }
};

FILTER_DESC clones[] =
{
	{ (0x00000000), "yes" },
	{ (0x00000001), "no" }
};

ROMLIST romlist;

unsigned int * romsort;
int current_filter;
int current_hardware;
int current_genre;
int current_clone;

unsigned int unfiltered_nb_list[NB_FILTERS];
unsigned int * unfiltered_romsort[NB_FILTERS];

std::set<unsigned int> favorite;
int favorite_changed = 1;

void load_favorite()
{
	FILE *fp;
	char filename[512];
	char ligne[256];
	unsigned int romnum;

	sprintf(filename, "%s/favorite.cfg", szAppHomePath);
	
	if((fp = fopen(filename, "r")) != NULL) {
		favorite.clear();
		while(fgets(ligne, sizeof(ligne), fp) != NULL) {
			sscanf(ligne, "%u", &romnum);
			favorite.insert(romnum);
		}
		fclose(fp);
		favorite_changed = 1;
	}
}

void save_favorite()
{
	FILE *fp;
	char filename[512];
	char g_string[255];

	sprintf(filename, "%s/favorite.cfg", szAppHomePath);
	
	fp = fopen(filename, "w");
	if (fp != NULL) {
		for (std::set<unsigned int>::iterator it = favorite.begin();
			it != favorite.end(); it++) {
			sprintf((char*)g_string, "%u\n", *it);
			fputs(g_string, fp);
		}
		fclose(fp);
	}
}

static inline void load_last_romlist(std::unordered_set<std::string> & romset)
{
	romset.clear();
	char filename[MAX_PATH];
	sprintf(filename, "%s/lastromlist.txt", szAppHomePath);
	FILE * fp = fopen(filename, "r");
	if(!fp) return;
	char txtline[256];
	while(fgets(txtline, sizeof(txtline), fp) != NULL) {
		size_t sz = strlen(txtline);
		if(!sz) continue;
		for(int i=sz-1; i>=0; --i)
			if(txtline[i]=='\n')
			{
				txtline[i] = 0;
				break;
			}
		if(strlen(txtline))
			romset.insert(txtline);
	}
	fclose(fp);
}

static inline void save_last_romlist(std::unordered_set<std::string> & romset)
{
	char filename[MAX_PATH];
	sprintf(filename, "%s/lastromlist.txt", szAppHomePath);
	FILE * fp = fopen(filename, "w");
	if(!fp) {
		romset.clear();
		return;
	}
	for(auto & oneline : romset) {
		fputs(oneline.c_str(), fp);
		fputc('\n', fp);
	}
	fclose(fp);
	romset.clear();
}

static std::string get_romsdir_string()
{
	char dir_path[MAX_PATH];
	std::ostringstream ss;
	for(int i = 0; i < DIRS_MAX; i++) {
		struct stat stbuf;
		if(0==stat(szAppRomPaths[i], &stbuf) ) {
			ss << i << "#" << (uint32_t) stbuf.st_mtime << "$";
		}
	}
	return ss.str();
}

static std::string load_romsdir_string()
{
	char filename[MAX_PATH];
	sprintf(filename, "%s/romsdirstat.txt", szAppHomePath);
	std::ifstream t(filename);

	if(!t) return "";

	std::string str;

	t.seekg(0, std::ios::end);
	str.reserve(t.tellg());
	t.seekg(0, std::ios::beg);

	str.assign((std::istreambuf_iterator<char>(t)),
			   std::istreambuf_iterator<char>());
	return str;
}

static void save_romsdir_string(const std::string & str)
{
	if(!str.length()) return;
	char filename[MAX_PATH];
	sprintf(filename, "%s/romsdirstat.txt", szAppHomePath);
	std::ofstream t(filename);
	if(!t) return;
	t << str;
}

void gui_sort_romlist()
{
options.create_lists = 0;

	bool use_last_romlist = false;
	std::unordered_set<std::string> romset;
	if(!options.create_lists)
	{
		load_last_romlist(romset);
		if(romset.size())
			use_last_romlist = true;
	}

	std::string curm = get_romsdir_string();
	std::string past = load_romsdir_string();

	if(!curm.length() || !past.length() || curm !=  past)
	{
		use_last_romlist = false;
		save_romsdir_string(curm);
	}

	for (int i = 0; i < NB_FILTERS; i++)
	{
		unfiltered_romsort[i] = (unsigned int *)malloc(NB_MAX_GAMES * sizeof(unsigned int));
	}

	char g_string[2048];

	romlist.nb_list[0] = 0;
	romlist.long_max = 0;
	romlist.nb_rom = 0;

	for(int i = 0; i < nBurnDrvCount; i++) {
		// save it!
		nBurnDrvActive /*= nBurnDrvSelect[0]*/ = i;

		romlist.zip[i] = BurnDrvGetTextA(DRV_NAME);
		if(!(romlist.parent[i] = BurnDrvGetTextA(DRV_PARENT)))
			romlist.parent[i] = "fba";
		romlist.name[i] = BurnDrvGetTextA(DRV_FULLNAME);
		romlist.year[i] = BurnDrvGetTextA(DRV_DATE);
		romlist.manufacturer[i] = BurnDrvGetTextA(DRV_MANUFACTURER);
		romlist.hardware[i] = BurnDrvGetHardwareCode();
		romlist.genre[i] = BurnDrvGetGenreFlags();
		romlist.longueur[i] = strlen(romlist.name[i]);
		if(romlist.long_max < romlist.longueur[i] ) romlist.long_max = romlist.longueur[i];

		romlist.etat[i] = ROUGE;
		for(int j = 0; j < DIRS_MAX; j++) {
			if(strlen(szAppRomPaths[j]) > 0) {
				if(use_last_romlist) {
					if( romset.find(romlist.zip[i]) != romset.end() ) {
						romlist.etat[i] = (BurnDrvGetTextA(DRV_PARENT) ? ORANGE : JAUNE);
						++romlist.nb_rom;
						break;
					}
				}
				else {
					sprintf(g_string, "%s%s.zip", szAppRomPaths[j], romlist.zip[i]);
					if(0 == access(g_string, R_OK)) {
						romlist.etat[i] = (BurnDrvGetTextA(DRV_PARENT) ? ORANGE : JAUNE);
						++romlist.nb_rom;
						romset.insert(romlist.zip[i]);
						break;
					}
				}
			}
		}
		unfiltered_romsort[0][i] = i;
	}

	if(options.create_lists || !use_last_romlist)
		save_last_romlist(romset);

	romlist.nb_list[0] = nBurnDrvCount;

	std::sort(unfiltered_romsort[0], unfiltered_romsort[0] + romlist.nb_list[0], 
		[](unsigned int v1, unsigned int v2) {
			return (strcmp(romlist.name[v1], romlist.name[v2]) <= 0);
		});

	romlist.nb_list[1] = 0;
	romlist.nb_list[2] = 0;
	romlist.nb_list[3] = 0;
	romlist.nb_list[4] = 0;

	for (int i = 0; i < romlist.nb_list[0]; ++i) {
		if(romlist.etat[unfiltered_romsort[0][i]] == ROUGE) {
			unfiltered_romsort[1][romlist.nb_list[1]] = unfiltered_romsort[0][i];
			++romlist.nb_list[1];
		} else {
			unfiltered_romsort[2][romlist.nb_list[2]] = unfiltered_romsort[0][i];
			++romlist.nb_list[2];

			if(romlist.etat[unfiltered_romsort[0][i]] == VERT || romlist.etat[unfiltered_romsort[0][i]] == BLEU ) {
				unfiltered_romsort[3][romlist.nb_list[3]] = unfiltered_romsort[0][i];
				++romlist.nb_list[3];
			}
		}
	}

	printf("romlist.nb_rom=%i\n", romlist.nb_rom);
	printf("romlist.nb_list[0]=%i\n", romlist.nb_list[0]);
	printf("romlist.nb_list[1]=%i\n", romlist.nb_list[1]);
	printf("romlist.nb_list[2]=%i\n", romlist.nb_list[2]);
	printf("romlist.nb_list[3]=%i\n", romlist.nb_list[3]);
	printf("romlist.nb_list[4]=%i\n", romlist.nb_list[4]);

	for (int i = 0; i < NB_FILTERS; i++)
	{
		unfiltered_nb_list[i] = romlist.nb_list[i];
	}

	load_favorite();
}

unsigned int * gui_get_filtered_romsort(int filter, int hardware, int genre, int clone)
{
	if (filter < 0 || filter >= NB_FILTERS) filter = 0;
	if (hardware < 0 || filter >= NB_HARDWARES) hardware = 0;
	if (genre < 0 || filter >= NB_GENRES) genre = 0;
	if (clone < 0 || clone >= NB_CLONES) clone = 0;

	if (romsort == NULL ||
		current_filter != filter || current_hardware != hardware || current_genre != genre || current_clone != clone ||
		(filter == 4 && favorite_changed == 1))
	{
		current_filter = filter;
		current_hardware = hardware;
		current_genre = genre;
		current_clone = clone;
		if (filter == 4 && favorite_changed == 1)
			favorite_changed = 0;
		
		if (romsort != NULL)
		{
			free(romsort);
			romsort = NULL;
		}
		
		romsort = (unsigned int *)malloc(NB_MAX_GAMES * sizeof(unsigned int));
		if (filter != 4)
		{
			memcpy(romsort, unfiltered_romsort[filter], NB_MAX_GAMES * sizeof(unsigned int));
			romlist.nb_list[filter] = unfiltered_nb_list[filter];
		}
		else
		{
			// favorite
			memcpy(romsort, unfiltered_romsort[0], NB_MAX_GAMES * sizeof(unsigned int));
			romlist.nb_list[filter] = unfiltered_nb_list[0];
			
			int counter = 0;
			for (int i = 0; i < romlist.nb_list[filter]; i++)
			{
				if (favorite.find(romsort[i]) != favorite.end())
				{
					romsort[counter] = romsort[i];
					counter++;
				}
			}
			romlist.nb_list[filter] = counter;
			unfiltered_nb_list[filter] = counter;
		}
		
		// clone
		if (current_clone == 1)
		{
			int counter = 0;
			char fba[] = "fba";
			size_t fba_size = sizeof(fba);
			for (int i = 0; i < romlist.nb_list[filter]; i++)
			{
				if (memcmp(romlist.parent[romsort[i]], fba, fba_size) == 0)
				{
					romsort[counter] = romsort[i];
					counter++;
				}
			}

			romlist.nb_list[filter] = counter;
		}

		// hardware
		if (current_hardware != 0)
		{
			int counter = 0;
			for (int i = 0; i < romlist.nb_list[filter]; i++)
			{
				if ((((romlist.hardware[romsort[i]] | HARDWARE_PREFIX_CARTRIDGE) ^ HARDWARE_PREFIX_CARTRIDGE)
					& 0xff000000) == hardwares[current_hardware].code)
				{
					romsort[counter] = romsort[i];
					counter++;
				}
			}

			romlist.nb_list[filter] = counter;
		}

		// genrer
		if (current_genre != 0)
		{
			int counter = 0;
			for (int i = 0; i < romlist.nb_list[filter]; i++)
			{
				if ((romlist.genre[romsort[i]] & genres[current_genre].code) == genres[current_genre].code)
				{
					romsort[counter] = romsort[i];
					counter++;
				}
			}

			romlist.nb_list[filter] = counter;
		}
	}
	
	return romsort;
}

void add_to_favorite(unsigned int rom)
{
	favorite.insert(rom);
	favorite_changed = 1;
}

void gui_add_to_favorite(unsigned int rom)
{
	add_to_favorite(rom);
	save_favorite();
}

void remove_from_favorite(unsigned int rom)
{
	favorite.erase(rom);
	favorite_changed = 1;
}

void gui_remove_from_favorite(unsigned int rom)
{
	remove_from_favorite(rom);
	save_favorite();
}

void gui_clear_favorite(bool missing)
{
	if (!missing) {
		favorite.clear();
	}
	else {
		for (std::set<unsigned int>::iterator it = favorite.begin(); it != favorite.end(); ) {
			if (romlist.etat[*it] == ROUGE) {
				favorite.erase(it++);
			}
			else {
				++it;
			}
		}
	}
	favorite_changed = 1;
	save_favorite();
}

bool gui_in_favorite(unsigned int rom)
{
	return favorite.find(rom) != favorite.end();
}

void gui_favorite_change(unsigned int rom)
{
	if (gui_in_favorite(rom))
	{
		gui_remove_from_favorite(rom);
	}
	else
	{
		gui_add_to_favorite(rom);
	}
}
