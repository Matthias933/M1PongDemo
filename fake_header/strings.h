/*
 * strings.h
 *
 *  Created on: Sep 27, 2018
 *      Author: duco
 */

#ifndef FAKE_HEADER_STRINGS_H_
#define FAKE_HEADER_STRINGS_H_

#ifndef STRNCASECMP
#define STRNCASECMP(s1, s2, sz) strnicmp((s1), (s2), (sz))
#endif

#ifndef STRCASECMP
#define STRCASECMP(s1, s2)      stricmp((s1), (s2))
#endif

#endif /* FAKE_HEADER_STRINGS_H_ */
