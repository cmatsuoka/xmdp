#include <stdio.h>
#include <dirent.h>
#include "mdp.h"

#define LINE_SIZE 80

struct menu menu;


int match_filename(char *filename, char *name, size_t n)
{
	DIR *d;
	struct dirent *ent;

	if ((d = opendir(".")) == NULL) {
		return -1;
	}

	while ((ent = readdir(d)) != NULL) {
		if (!strcasecmp(filename, ent->d_name)) {
			strncpy(name, ent->d_name, n);
			closedir(d);
			return 0;
		}
	}

	strncpy(name, filename, n);
	closedir(d);

	return -1;

}

static char *copy_trim(char *s)
{
	int len = strlen(s);

	if (len > 0 && s[len - 1] == '\n') {
		s[len - 1] = 0;
	}
	if (len > 1 && s[len - 2] == '\r') {
		s[len - 2] = 0;
	}

	return strdup(s);
}

static void collect_ystart()
{
	struct menu_entry *e;
	int i, j, ypos;

	ypos = 0;

	/* write titles */
	for (i = 0; menu.titles[i] && i < MAX_TITLES; i++) {
		ypos += 20;
	}

	ypos += 5;

	/* write entries */
	for (j = 0; menu.entry[j].filename && j < MAX_ENTRIES; j++) {
		ypos += 40;
		e = &menu.entry[j];

		e->ystart = ypos - 18;

		ypos += 4;
		for (i = 0; e->comment[i] && i < MAX_COMMENT; i++) {
			ypos += 18;
		}

		e->yend = ypos + 8;
	}
}

int parse_mdi()
{
	FILE *f;
	char line[LINE_SIZE];
	char name[FILENAME_SIZE];
	int i, j;

	match_filename("mdp.mdi", name, FILENAME_SIZE);
	
	f = fopen(name, "r");
	if (f == NULL) {
		perror("xmdp");
		return -1;
	}

	/* read title */
	for (i = 0; i < MAX_COMMENT; i++) {
		fgets(line, LINE_SIZE, f);
		if (line[0] == '#') {
			break;
		}
		menu.titles[i] = copy_trim(line);
		/* printf("TITLE: %s\n", menu.titles[i]); */
	}
	if (line[0] != '#') {
		return -1;
	}

	/* read entries */
	for (i = 0; i < MAX_ENTRIES; i++) {
		if (line[1] == '#') {
			break;
		}

		menu.num_entries++;
		menu.entry[i].filename = copy_trim(line + 1);
		/* printf("filename #%d: %s\n", i, menu.entry[i].filename); */
		fgets(line, LINE_SIZE, f);
		menu.entry[i].title = copy_trim(line);
		fgets(line, LINE_SIZE, f);
		menu.entry[i].year = copy_trim(line);

		for (j = 0; j < MAX_COMMENT; j++) {
			fgets(line, LINE_SIZE, f);
			if (line[0] == '#') {
				break;
			}
			menu.entry[i].comment[j] = copy_trim(line);
		}
		if (line[0] != '#') {
			return -1;
		}
	}
	if (line[1] != '#') {
		return -1;
	}

	fgets(line, LINE_SIZE, f);
	menu.lastline = copy_trim(line);

	collect_ystart();

	return 0;
}

