/* ner: src/notmuch.cc
 *
 * Copyright (c) 2010 Michael Forney
 *
 * This file is a part of ner.
 *
 * ner is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License version 3, as published by the Free
 * Software Foundation.
 *
 * ner is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * ner.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "notmuch.hh"

std::string NotMuch::_path;

void NotMuch::setDatabasePath(const std::string & path)
{
    _path = path;
}

notmuch_database_t * NotMuch::openDatabase(notmuch_database_mode_t mode)
{
    return notmuch_database_open(_path.c_str(), mode);
}

// vim: fdm=syntax fo=croql et sw=4 sts=4 ts=8

