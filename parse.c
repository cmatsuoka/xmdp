#include <stdio.h>
#include "mdp.h"

#define LINE_SIZE 80

struct menu menu;

static char *copy_trim(char *s)
{
	int len = strlen(s);

	if (len > 0 && s[len - 1] == '\n') {
		s[len - 1] = 0;
	}

	return strdup(s);
}

int parse_mdi()
{
	FILE *f;
	char line[LINE_SIZE];
	int i, j;

	f = fopen("mdp.mdi", "r");
	if (f == NULL) {
		perror("xmdp");
		return -1;
	}

	// read title
	for (i = 0; i < MAX_COMMENT; i++) {
		fgets(line, LINE_SIZE, f);
		if (line[0] == '#') {
			break;
		}
		menu.titles[i] = copy_trim(line);
		printf("TITLE: %s\n", menu.titles[i]);
	}
	if (line[0] != '#') {
		return -1;
	}

	for (i = 0; i < MAX_ENTRIES; i++) {
		if (line[1] == '#') {
			break;
		}

		menu.num_entries++;
		menu.entry[i].filename = copy_trim(line + 1);
		printf("filename #%d: %s\n", i, menu.entry[i].filename);
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


	return 0;
}

