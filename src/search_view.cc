/* ner: src/search_view.cc
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

#include <iostream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <iterator>
#include <sched.h>

#include "search_view.hh"
#include "thread_message_view.hh"
#include "view_manager.hh"
#include "util.hh"
#include "colors.hh"
#include "ncurses.hh"
#include "notmuch.hh"
#include "status_bar.hh"
#include "line_editor.hh"
#include "ner_config.hh"

const int newestDateWidth = 13;
const int messageCountWidth = 8;
const int authorsWidth = 20;

const auto conditionWaitTime = std::chrono::milliseconds(50);

SearchView::SearchView(const std::string & search, const View::Geometry & geometry)
    : LineBrowserView(geometry),
        _searchTerms(search)
{
    _collecting = true;
    _thread = std::thread(std::bind(&SearchView::collectThreads, this));

    std::map<std::string, std::string> _keymap = NerConfig::instance().getSearchKeyMap();
    std::map<std::string, std::string> _generalKeymap = NerConfig::instance().getGeneralKeyMap();

    /* Key Sequences */
    if (_keymap.count("refreshThreads") == 1)
	addHandledSequence(_keymap.find("refreshThreads")->second, std::bind(&SearchView::refreshThreads, this));
    else
	addHandledSequence("=", std::bind(&SearchView::refreshThreads, this));
    if (_generalKeymap.count("open") == 1)
	addHandledSequence(_generalKeymap.find("open")->second, std::bind(&SearchView::openSelectedThread, this));
    else
	addHandledSequence("\n", std::bind(&SearchView::openSelectedThread, this));

    if (_generalKeymap.count("archiveThread") == 1)
	addHandledSequence(_generalKeymap.find("archiveThread")->second, std::bind(&SearchView::archiveSelectedThread, this));
    else
	addHandledSequence("a", std::bind(&SearchView::archiveSelectedThread, this));

    if (_generalKeymap.count("addTags") == 1)
	addHandledSequence(_generalKeymap.find("addTags")->second, std::bind(&SearchView::addTags, this));
    else
	addHandledSequence("+", std::bind(&SearchView::addTags, this));
    if (_generalKeymap.count("removeTags") == 1)
	addHandledSequence(_generalKeymap.find("removeTags")->second, std::bind(&SearchView::removeTags, this));
    else
	addHandledSequence("-", std::bind(&SearchView::removeTags, this));

    std::unique_lock<std::mutex> lock(_mutex);
    while (_threads.size() < getmaxy(_window) && _collecting)
        _condition.wait_for(lock, conditionWaitTime);
}

SearchView::~SearchView()
{
    if (_thread.joinable())
    {
        _collecting = false;
        _thread.join();
    }
}

void SearchView::update()
{
    werase(_window);

    if (_offset > _threads.size())
        return;

    int row = 0;

    for (auto thread = _threads.begin() + _offset;
        thread != _threads.end() && row < getmaxy(_window);
        ++thread, ++row)
    {
        bool selected = row + _offset == _selectedIndex;
        bool unread = thread->tags.find("unread") != thread->tags.end();
        bool completeMatch = thread->matchedMessages == thread->totalMessages;

        int x = 0;

        wmove(_window, row, x);

        attr_t attributes = 0;

        if (unread)
            attributes |= A_BOLD;

        if (selected)
            attributes |= A_REVERSE;

        wchgat(_window, -1, attributes, 0, NULL);

        try
        {
            /* Date */
            NCurses::addPlainString(_window, relativeTime(thread->newestDate),
                attributes, ColorID::SearchViewDate, newestDateWidth - 1);

            NCurses::checkMove(_window, x += newestDateWidth);

            /* Message Count */
            std::ostringstream messageCountStream;
            messageCountStream << thread->matchedMessages << '/' << thread->totalMessages;

            x += NCurses::addChar(_window, '[', attributes);
            NCurses::checkMove(_window, x);

            x += NCurses::addPlainString(_window, messageCountStream.str(),
                attributes, completeMatch ? ColorID::SearchViewMessageCountComplete :
                                            ColorID::SearchViewMessageCountPartial,
                messageCountWidth - 1);
            NCurses::checkMove(_window, x);

            NCurses::addChar(_window, ']', attributes);

            NCurses::checkMove(_window, x = newestDateWidth + messageCountWidth);

            /* Authors */
            NCurses::addUtf8String(_window, thread->authors.c_str(),
                attributes, ColorID::SearchViewAuthors, authorsWidth - 1);

            NCurses::checkMove(_window, x += authorsWidth);

            /* Subject */
            x += NCurses::addUtf8String(_window, thread->subject.c_str(),
                attributes, ColorID::SearchViewSubject);

            NCurses::checkMove(_window, ++x);

            /* Tags */
            std::ostringstream tagStream;
            std::copy(thread->tags.begin(), thread->tags.end(),
                std::ostream_iterator<std::string>(tagStream, " "));
            std::string tags(tagStream.str());

            if (tags.size() > 0)
                /* Get rid of the trailing space */
                tags.resize(tags.size() - 1);

            x += NCurses::addPlainString(_window, tags, attributes, ColorID::SearchViewTags);

            NCurses::checkMove(_window, x - 1);
        }
        catch (const NCurses::CutOffException & e)
        {
            NCurses::addCutOffIndicator(_window, attributes);
        }
    }
}

std::vector<std::string> SearchView::status() const
{
    std::ostringstream threadPosition;

    if (_threads.size() > 0)
        threadPosition << "thread " << (_selectedIndex + 1) << " of " << _threads.size();
    else
        threadPosition << "no matching threads";

    return std::vector<std::string>{
        "search-terms: \"" + _searchTerms + '"',
        threadPosition.str()
    };
}

void SearchView::openSelectedThread()
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_selectedIndex < _threads.size())
    {
        try
        {
            ViewManager::instance().addView(std::make_shared<ThreadMessageView>(
                _threads.at(_selectedIndex).id));
        }
        catch (const InvalidThreadException & e)
        {
            StatusBar::instance().displayMessage(e.what());
        }
        catch (const InvalidMessageException & e)
        {
            StatusBar::instance().displayMessage(e.what());
        }
    }
}

void SearchView::archiveSelectedThread()
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_selectedIndex < _threads.size())
    {
        try
        {
            _threads.at(_selectedIndex).removeTag("inbox");

            next();
            update();
        }
        catch (const InvalidThreadException & e)
        {
            StatusBar::instance().displayMessage(e.what());
        }
        catch (const InvalidMessageException & e)
        {
            StatusBar::instance().displayMessage(e.what());
        }
    }
}

void SearchView::refreshThreads()
{
    /* If the thread is still going, stop it, and wait for it to return */
    if (_thread.joinable())
    {
        _collecting = false;
        _thread.join();
    }

    bool empty = _threads.empty();
    std::string selectedId;

    if (!empty)
        selectedId = (*(_threads.begin() + _selectedIndex)).id;

    _threads.clear();

    /* Start collecting threads in the background */
    _collecting = true;
    _thread = std::thread(std::bind(&SearchView::collectThreads, this));

    /* Locate the previously selected thread ID */
    bool found = false;
    std::unique_lock<std::mutex> lock(_mutex);

    if (empty)
        found = true;
    else
    {
        int index = 0;

        while (!found && _collecting)
        {
            for (; index < _threads.size(); ++index)
            {
                /* Stop if we found the thread ID */
                if (_threads.at(index).id == selectedId)
                {
                    found = true;
                    _selectedIndex = index;
                    break;
                }
            }

            _condition.wait_for(lock, conditionWaitTime);
        }
    }

    /* Wait until we have enough threads to fill the screen */
    while (_threads.size() - _offset < getmaxy(_window) && _collecting)
        _condition.wait_for(lock, conditionWaitTime);

    /* If we didn't find it, make sure the selected index is valid */
    if (!found)
    {
        if (_threads.size() <= _selectedIndex)
            _selectedIndex = _threads.size() - 1;
    }

    StatusBar::instance().update();
    makeSelectionVisible();
}

int SearchView::lineCount() const
{
    return _threads.size();
}

void SearchView::collectThreads()
{
    std::unique_lock<std::mutex> lock(_mutex);
    lock.unlock();

    notmuch_database_t * database = Notmuch::readonlyDatabase();
    notmuch_query_t * query = notmuch_query_create(database, _searchTerms.c_str());
    notmuch_query_set_sort(query, NerConfig::instance().sortMode());
    notmuch_threads_t * threadIterator;

    for (threadIterator = notmuch_query_search_threads(query);
        notmuch_threads_valid(threadIterator) && _collecting;
        notmuch_threads_move_to_next(threadIterator))
    {
        lock.lock();

        notmuch_thread_t * thread = notmuch_threads_get(threadIterator);
        _threads.push_back(Thread(thread));

        _condition.notify_one();

        lock.unlock();

        sched_yield();
    }

    _collecting = false;
    notmuch_query_destroy(query);
    notmuch_database_close(database);

    /* For cases when there are no matching threads */
    _condition.notify_one();
}

void SearchView::addTags()
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_selectedIndex < _threads.size())
    {
        Thread & thread = _threads.at(_selectedIndex);

        try
        {
            std::string tags = StatusBar::instance().prompt("Tags: ", "tags");

            if (!tags.empty()) {
                std::stringstream ss(tags);
                std::string s;

                while (std::getline(ss, s, ' ')) {
                    thread.addTag(s);
                }

                next();
                update();
            }
        }
        catch (const AbortInputException&)
        {
        }
    }
}

void SearchView::removeTags()
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_selectedIndex < _threads.size())
    {
        Thread & thread = _threads.at(_selectedIndex);

        try
        {
            std::string tags = StatusBar::instance().prompt("Tags: ", "tags");

            if (!tags.empty()) {
                std::stringstream ss(tags);
                std::string s;

                while (std::getline(ss, s, ' ')) {
                    thread.removeTag(s);
                }

                next();
                update();
            }
        }
        catch (const AbortInputException&)
        {
        }
    }
}

// vim: fdm=syntax fo=croql et sw=4 sts=4 ts=8

