/* Copyright (c) 2007, Jan Wolter, All Rights Reserved */
/* Copyright 2007 Jan Wolter
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "pbnsolve.h"

/* This is a rather minimalistic hunk of code to parse HTTP query strings. */

char x2c(char *what);
void unescape(char *url);
void plustospace(char *str);
char *get_multipart_query(int cl, char *ct, FILE *repfp);
char *get_xmlrpc_query(int len);


/* GET_QUERY - fetch the raw query string and return it.  It is taken either
 * from the QUERY_STRING or from stdin, depending on weather the
 * REQUEST_METHOD is POST or GET.  If it was POST the returned query string is
 * in malloc'ed memory, if it was GET it isn't.  Rather a defect, if you ask me.
 */

char *get_query()
{
    char *query;
    char *method= getenv("REQUEST_METHOD");

    if (method != NULL && !strcmp(method,"POST"))
    {
	char *cl= getenv("CONTENT_LENGTH");
	int len= (cl == NULL) ? 0 : atol(cl);
	query= (char *)malloc(len+1);
	len= fread(query,1,len,stdin);
	query[len]= '\0';
    }
    else
	query= getenv("QUERY_STRING");

    return query;
}


/* QUERY_LOOKUP - Look up the value of the give variable in the query string.
 * returns the value in malloc'ed memory.  Returns NULL if it is not defined.
 * Return "" if it is defined but has no value.
 */

char *query_lookup(char *query, char *var)
{
    char *end, *v, *t, *term;

    if  (query == NULL || query[0] == '\0')
	return NULL;
    
    for (;;)
    {
	if ((end= strchr(query,'&')) != NULL) *end= '\0';
	term= strdup(query);

	plustospace(term);
	unescape(term);

	if ((v= strchr(term,'=')) != NULL) *v= '\0';
	if (!strcmp(term, var))
	{
	    if (v == NULL)
	    	v= strdup("");
	    else
	    	v= strdup(v+1);
	    free(term);
	    if (end != NULL) *end= '&';
	    return v;
	}
	if (end == NULL) break;
	*end= '&';
	query= end + 1;
    }

    return NULL;
}


/* ------- The rest of this is recycled as is from the NCSA stuff ------- */

char x2c(char *what) {
    register char digit;

    digit = (what[0] >= 'A' ? ((what[0] & 0xdf) - 'A')+10 : (what[0] - '0'));
    digit *= 16;
    digit += (what[1] >= 'A' ? ((what[1] & 0xdf) - 'A')+10 : (what[1] - '0'));
    return digit;
}

void unescape(char *url) {
    register int x,y;

    for(x=0,y=0;url[y];++x,++y) {
        if((url[x] = url[y]) == '%') {
            url[x] = x2c(&url[y+1]);
            y+=2;
        }
    }
    url[x] = '\0';
}

void plustospace(char *str) {
    register int x;

    for(x=0;str[x];x++) if(str[x] == '+') str[x] = ' ';
}
