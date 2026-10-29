#ifndef TEXTTEMPLATE_H
#define TEXTTEMPLATE_H

#include "PullDown.h"

extern int parse_gender[10];
extern int parse_plural[10];
extern int parsing_gender;
extern int parsing_plural;

extern int parse_numbers[10];

struct TemplateLink {
    char name[260];
    int value;
};
extern TemplateLink g_parse_links[10];

extern char tokens[6][20];

#endif
