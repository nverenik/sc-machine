/*
-----------------------------------------------------------------------------
This source file is part of OSTIS (Open Semantic Technology for Intelligent Systems)
For the latest info, see http://www.ostis.net

Copyright (c) 2010-2014 OSTIS

OSTIS is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

OSTIS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with OSTIS. If not, see <http://www.gnu.org/licenses/>.
-----------------------------------------------------------------------------
 */

#include "sc_memory_headers.h"

#include <glib.h>

#ifndef _merge_functions_h_
#define _merge_functions_h_

/*!
 * Remove question from question_initiated set and append it into
 * question_finished set.
 * @param question sc-addr of question node
 */
void finish_question(sc_addr question);

/*!
 * Copies аll connectors which are incident to given sc-element to the
 * the another sc-element
 * @param dest sc-element, to which connectors are copied
 * @param source sc-element, from which connectors are copied
 */
void copy_incident_connectors(sc_addr dest, sc_addr source);

/*!
 * Merges two given sc-elements
 * All connectors are copied to the first element,
 * the second element is deleted after copying
 * @param first First sc-element to merge
 * @param second Second sc-element to merge
 * @returns The result of second sc-element deletion
 */
sc_result merge_sc_elements(sc_addr first, sc_addr second);

/*!
 * Resolves sc-addr seg and offset from pointer
 * @param data Given pointer
 * @returns Resolved sc-addr
 */
sc_addr resolve_sc_addr_from_pointer(gpointer data);

#endif
