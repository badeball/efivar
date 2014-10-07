/*
 * libefivar - library for the manipulation of EFI variables
 * Copyright 2012-2013 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>

#include "efivar.h"
#include "guid.h"

#define GUID_FORMAT "%08x-%04x-%04x-%04x-%02x%02x%02x%02x%02x%02x"
#define GUID_LENGTH_WITH_NUL 37

int
__attribute__ ((__nonnull__ (1, 2)))
efi_str_to_guid(const char *s, efi_guid_t *guid)
{
	return text_to_guid(s, guid);
}

int
__attribute__ ((__nonnull__ (1)))
efi_guid_to_str(const efi_guid_t *guid, char **sp)
{
	char *ret = NULL;
	int rc = -1;

	if (!sp) {
		return snprintf(NULL, 0, GUID_FORMAT,
				guid->a, guid->b, guid->c, bswap_16(guid->d),
				guid->e[0], guid->e[1], guid->e[2], guid->e[3],
				guid->e[4], guid->e[5]);
	} else if (sp && *sp) {
		return snprintf(*sp, GUID_LENGTH_WITH_NUL, GUID_FORMAT,
				guid->a, guid->b, guid->c, bswap_16(guid->d),
				guid->e[0], guid->e[1], guid->e[2], guid->e[3],
				guid->e[4], guid->e[5]);
	} else {
		rc = asprintf(&ret, GUID_FORMAT,
				guid->a, guid->b, guid->c, bswap_16(guid->d),
				guid->e[0], guid->e[1], guid->e[2], guid->e[3],
				guid->e[4], guid->e[5]);
		if (rc >= 0)
			*sp = ret;
	}
	return rc;
}

extern struct guidname efi_well_known_guids[], efi_well_known_names[];
extern char efi_well_known_guids_end, efi_well_known_names_end;

static int
__attribute__ ((__nonnull__ (1, 2)))
cmpguidp(const void *p1, const void *p2)
{
	struct guidname *gn1 = (struct guidname *)p1;
	struct guidname *gn2 = (struct guidname *)p2;

	return memcmp(&gn1->guid, &gn2->guid, sizeof (gn1->guid));
}

static int
__attribute__ ((__nonnull__ (1, 2)))
cmpnamep(const void *p1, const void *p2)
{
	struct guidname *gn1 = (struct guidname *)p1;
	struct guidname *gn2 = (struct guidname *)p2;

	return memcmp(gn1->name, gn2->name, sizeof (gn1->name));
}

int
__attribute__ ((__nonnull__ (1, 2)))
efi_guid_to_name(efi_guid_t *guid, char **name)
{
	intptr_t end = (intptr_t)&efi_well_known_guids_end;
	intptr_t start = (intptr_t)&efi_well_known_guids;
	size_t nmemb = (end - start) / sizeof (efi_well_known_guids[0]);

	struct guidname key;
	memset(&key, '\0', sizeof (key));
	memcpy(&key.guid, guid, sizeof (*guid));

	struct guidname *result;
	result = bsearch(&key, efi_well_known_guids, nmemb,
			sizeof (efi_well_known_guids[0]), cmpguidp);
	if (result != NULL) {
		*name = strndup(result->name, sizeof (result->name) -1);
		return *name ? (int)strlen(*name) : -1;
	}
	return efi_guid_to_str(guid, name);
}

int
__attribute__ ((__nonnull__ (1, 2)))
efi_guid_to_symbol(efi_guid_t *guid, char **symbol)
{
	intptr_t end = (intptr_t)&efi_well_known_guids_end;
	intptr_t start = (intptr_t)&efi_well_known_guids;
	size_t nmemb = (end - start) / sizeof (efi_well_known_guids[0]);

	struct guidname key;
	memset(&key, '\0', sizeof (key));
	memcpy(&key.guid, guid, sizeof (*guid));

	struct guidname *result;
	result = bsearch(&key, efi_well_known_guids, nmemb,
			sizeof (efi_well_known_guids[0]), cmpguidp);
	if (result != NULL) {
		*symbol = strndup(result->symbol, sizeof (result->symbol) -1);
		return *symbol ? (int)strlen(*symbol) : -1;
	}
	errno = EINVAL;
	return -1;
}

int
__attribute__ ((__nonnull__ (1, 2)))
efi_symbol_to_guid(const char *symbol, efi_guid_t *guid)
{
	void *dlh = dlopen(NULL, RTLD_LAZY);
	if (!dlh)
		return -1;

	void *sym = dlsym(dlh, symbol);
	dlclose(dlh);
	if (!sym)
		return -1;

	memcpy(guid, sym, sizeof(*guid));
	return 0;
}

int
__attribute__ ((__nonnull__ (1, 2)))
efi_name_to_guid(const char *name, efi_guid_t *guid)
{
	intptr_t end = (intptr_t)&efi_well_known_names_end;
	intptr_t start = (intptr_t)&efi_well_known_names;
	size_t nmemb = (end - start) / sizeof (efi_well_known_names[0]);
	size_t namelen = strnlen(name, 39);

	struct guidname key;
	memset(&key, '\0', sizeof (key));
	memcpy(key.name, name, namelen);

	if (namelen > 2 && name[0] == '{' && name[namelen] == '}') {
		namelen -= 2;
		memcpy(key.name, name + 1, namelen);
		memset(key.name + namelen, '\0', sizeof(key) - namelen);
	}

	key.name[sizeof(key.name) - 1] = '\0';

	struct guidname *result;
	result = bsearch(&key, efi_well_known_names, nmemb,
			sizeof (efi_well_known_names[0]), cmpnamep);
	if (result != NULL) {
		memcpy(guid, &result->guid, sizeof (*guid));
		return 0;
	} else {
		char tmpname[sizeof(key.name) + 9];
		strcpy(tmpname, "efi_guid_");
		memcpy(tmpname+9, key.name, sizeof (key.name));
		int rc = efi_symbol_to_guid(tmpname, guid);
		if (rc >= 0)
			return rc;
	}

	errno = ENOENT;
	return -1;
}
