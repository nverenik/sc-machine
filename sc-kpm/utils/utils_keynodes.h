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

#ifndef _utils_keynodes_h_
#define _utils_keynodes_h_

#include "sc_memory.h"

extern sc_addr keynode_nrel_idtf;
extern sc_addr keynode_nrel_main_idtf;
extern sc_addr keynode_nrel_system_identifier;
extern sc_addr keynode_system_element;

extern sc_addr keynode_sc_garbage;

sc_result utils_collect_keynodes_initialize();

sc_result utils_keynodes_initialize();

#endif
