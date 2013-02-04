/* ner: src/ner.cc
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
#include <sys/types.h>
#include <signal.h>

#include "ner.hh"
#include "ncurses.h"
#include "util.hh"
#include "status_bar.hh"
#include "view_manager.hh"
#include "search_view.hh"
#include "message_view.hh"
#include "thread_view.hh"
#include "view_view.hh"
#include "compose_view.hh"
#include "colors.hh"
#include "notmuch.hh"
#include "line_editor.hh"
#include "message.hh"

Ner::Ner()
{
    std::map<std::string, std::string> _keymap = NerConfig::instance().getMainKeyMap();

    /* Key Sequences */
    if (_keymap.count("Quit") == 1)
	addHandledSequence(_keymap.find("Quit")->second, std::bind(&Ner::quit, this));
    else
	addHandledSequence("Q", std::bind(&Ner::quit, this));

    if (_keymap.count("search") == 1)
	addHandledSequence(_keymap.find("search")->second, std::bind(&Ner::search, this));
    else
	addHandledSequence("s", std::bind(&Ner::search, this));

    if (_keymap.count("compose") == 1)
	addHandledSequence(_keymap.find("compose")->second, std::bind(&Ner::compose, this));
    else
	addHandledSequence("m", std::bind(&Ner::compose, this));

    if (_keymap.count("openMessage") == 1)
        addHandledSequence(_keymap.find("openMessage")->second, std::bind(&Ner::openMessage, this));
    else
	addHandledSequence("M", std::bind(&Ner::openMessage, this));

    if (_keymap.count("openThread") == 1)
        addHandledSequence(_keymap.find("openThread")->second, std::bind(&Ner::openThread, this));
    else
	addHandledSequence("T", std::bind(&Ner::openThread, this));

    if (_keymap.count("openViewView") == 1)
        addHandledSequence(_keymap.find("openViewView")->second, std::bind(&Ner::openViewView, this));
    else
	addHandledSequence(";", std::bind(&Ner::openViewView, this));

    if (_keymap.count("redraw") == 1)
        addHandledSequence(_keymap.find("redraw")->second, std::bind(&Ner::redraw, this));
    else
	addHandledSequence("<C-l>", std::bind(&Ner::redraw, this));

    if (_keymap.count("sigstp") == 1)
        addHandledSequence(_keymap.find("sigstp")->second, std::bind(&kill, getpid(), SIGTSTP));
    else
	addHandledSequence("<C-z>", std::bind(&kill, getpid(), SIGTSTP));
}

Ner::~Ner()
{
}

void Ner::run()
{
    std::vector<int> sequence;

    _running = true;

    _viewManager.refresh();

    while (_running)
    {
        int key = getch();

        if (key == KEY_BACKSPACE && sequence.size() > 0)
            sequence.pop_back();
        else if (key == 'c' - 96) // Ctrl-C
            sequence.clear();
        else
        {
            sequence.push_back(key);

            auto handleResult = handleKeySequence(sequence);

            /* If Ner handled the input sequence */
            if (handleResult == InputHandler::HandleResult::Handled)
                sequence.clear();
            else
            {
                auto viewManagerHandleResult = _viewManager.handleKeySequence(sequence);

                /* If the ViewManager handled the input sequence, or neither
                 * Ner nor the ViewManager had a partial match with the input
                 * sequence */
                if (viewManagerHandleResult == InputHandler::HandleResult::Handled ||
                    (viewManagerHandleResult == InputHandler::HandleResult::NoMatch &&
                        handleResult == InputHandler::HandleResult::NoMatch))
                    sequence.clear();
            }
        }

        if (!_running)
            break;

        _viewManager.update();
        _viewManager.refresh();
    }
}

void Ner::quit()
{
    _running = false;
}

void Ner::search()
{
    try
    {
        std::string searchTerms = StatusBar::instance().prompt("Search: ", "search");

        if (!searchTerms.empty())
            _viewManager.addView(std::make_shared<SearchView>(searchTerms));
    }
    catch (const AbortInputException&)
    { }
}

void Ner::compose()
{
    try
    {
        _viewManager.addView(std::make_shared<ComposeView>());
    }
    catch (const AbortInputException&)
    { }
}

void Ner::openMessage()
{
    std::string messageId = StatusBar::instance().prompt("Message ID: ", "message-id");

    if (!messageId.empty())
    {
        if (messageId.size() > 3 && std::equal(messageId.begin(), messageId.begin() + 3, "id:"))
            messageId.erase(0, 3);

        try
        {
            std::shared_ptr<MessageView> messageView(new MessageView());
            messageView->setMessage(messageId);
            _viewManager.addView(std::move(messageView));
        }
        catch (const InvalidMessageException & e)
        {
            StatusBar::instance().displayMessage(e.what());
        }
    }
}

void Ner::openThread()
{
    std::string threadId = StatusBar::instance().prompt("Thread ID: ", "thread-id");

    if (!threadId.empty())
    {
        try
        {
            _viewManager.addView(std::make_shared<ThreadView>(threadId));
        }
        catch (const InvalidThreadException & e)
        {
            _statusBar.displayMessage(e.what());
        }
    }
}

void Ner::openViewView()
{
    _viewManager.addView(std::make_shared<ViewView>());
}

void Ner::redraw()
{
    clear();
    refresh();

    _statusBar.update();
    _statusBar.refresh();
}

// vim: fdm=syntax fo=croql et sw=4 sts=4 ts=8

