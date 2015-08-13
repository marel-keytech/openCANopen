#ifndef INI_PARSER_H_INCLUDED_
#define INI_PARSER_H_INCLUDED_

#include <stdio.h>
#include "kv_dict.h"

int ini_parse(struct kv_dict* dict, FILE* stream);

#endif /* INI_PARSER_H_INCLUDED_ */

