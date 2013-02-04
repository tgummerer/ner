/* ner: src/line_browser_view.cc
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

#include <sstream>

#include "line_browser_view.hh"
#include "view_manager.hh"
#include "ner_config.hh"

LineBrowserView::LineBrowserView(const View::Geometry & geometry)
    : WindowView(geometry),
        _selectedIndex(0),
        _offset(0)
{
    std::map<std::string, std::string> _keymap = NerConfig::instance().getGeneralKeyMap();

    /* Key Sequences */
    if (_keymap.count("next") == 1)
	addHandledSequence(_keymap.find("next")->second, std::bind(&LineBrowserView::next, this));
    else
	addHandledSequence("<Down>", std::bind(&LineBrowserView::next, this));
    if (_keymap.count("previous") == 1)
	addHandledSequence(_keymap.find("previous")->second, std::bind(&LineBrowserView::previous, this));
    else
	addHandledSequence("<Up>", std::bind(&LineBrowserView::previous, this));

    if (_keymap.count("nextPage") == 1)
	addHandledSequence(_keymap.find("nextPage")->second, std::bind(&LineBrowserView::nextPage, this));
    else
	addHandledSequence("<PageDown>", std::bind(&LineBrowserView::nextPage, this));

    if (_keymap.count("previousPage") == 1)
	addHandledSequence(_keymap.find("previousPage")->second, std::bind(&LineBrowserView::previousPage, this));
    else
	addHandledSequence("<PageUp>", std::bind(&LineBrowserView::previousPage, this));

    if (_keymap.count("top") == 1)
	addHandledSequence(_keymap.find("top")->second, std::bind(&LineBrowserView::moveToTop, this));
    else
	addHandledSequence("<Home>", std::bind(&LineBrowserView::moveToTop, this));
    if (_keymap.count("bottom") == 1)
	addHandledSequence(_keymap.find("bottom")->second, std::bind(&LineBrowserView::moveToBottom, this));
    else
	addHandledSequence("<End>", std::bind(&LineBrowserView::moveToBottom, this));
}

void LineBrowserView::resize(const View::Geometry & geometry)
{
    WindowView::resize(geometry);

    makeSelectionVisible();
}

std::vector<std::string> LineBrowserView::status() const
{
    std::ostringstream position;

    if (lineCount() > 0)
        position << "line " << _selectedIndex + 1 << " of " << lineCount();
    else
        position << "no lines";

    return std::vector<std::string>{ position.str() };
}

void LineBrowserView::next()
{
    if (_selectedIndex + 1 < lineCount())
        ++_selectedIndex;

    makeSelectionVisible();
}

void LineBrowserView::previous()
{
    if (_selectedIndex > 0)
        --_selectedIndex;

    makeSelectionVisible();
}

void LineBrowserView::nextPage()
{
    if (_selectedIndex + visibleLines() >= lineCount())
        _selectedIndex = lineCount() - 1;
    else
        _selectedIndex += visibleLines() - 1;

    makeSelectionVisible();
}

void LineBrowserView::previousPage()
{
    if (getmaxy(_window) > _selectedIndex)
        _selectedIndex = 0;
    else
        _selectedIndex -= visibleLines() - 1;

    makeSelectionVisible();
}

void LineBrowserView::moveToTop()
{
    _selectedIndex = 0;

    makeSelectionVisible();
}

void LineBrowserView::moveToBottom()
{
    _selectedIndex = lineCount() - 1;

    makeSelectionVisible();
}

void LineBrowserView::makeSelectionVisible()
{
    if (_selectedIndex < _offset)
        _offset = _selectedIndex;
    else if (_selectedIndex >= _offset + visibleLines())
        _offset = _selectedIndex - visibleLines() + 1;

    StatusBar::instance().update();
    StatusBar::instance().refresh();
}

int LineBrowserView::visibleLines() const
{
    return getmaxy(_window);
}

// vim: fdm=syntax fo=croql et sw=4 sts=4 ts=8

