/* ner: src/thread_message_view.cc
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
#include <cstring>

#include "thread_message_view.hh"
#include "notmuch.hh"
#include "colors.hh"
#include "line_editor.hh"
#include "ner_config.hh"

const int threadViewHeight = 8;

ThreadMessageView::ThreadMessageView(const std::string & threadId, const View::Geometry & geometry)
    : _threadView(threadId, { geometry.x, geometry.y, geometry.width, threadViewHeight }),
        _messageView({
            geometry.x, geometry.y + threadViewHeight + 1,
            geometry.width, geometry.height - threadViewHeight - 1
        })
{
    loadSelectedMessage();

    std::map<std::string, std::string> _generalKeymap = NerConfig::instance().getGeneralKeyMap();
    std::map<std::string, std::string> _keymap = NerConfig::instance().getThreadViewKeyMap();

    /* Key Sequences */
    if (_generalKeymap.count("next") == 1)
	addHandledSequence(_generalKeymap.find("next")->second, std::bind(&MessageView::next, &_messageView));
    else
	addHandledSequence("<Down>", std::bind(&MessageView::next, &_messageView));
    if (_generalKeymap.count("previous") == 1)
	addHandledSequence(_generalKeymap.find("previous")->second, std::bind(&MessageView::previous, &_messageView));
    else
	addHandledSequence("<Up>", std::bind(&MessageView::previous, &_messageView));

    if (_generalKeymap.count("nextPage") == 1)
	addHandledSequence(_generalKeymap.find("nextPage")->second, std::bind(&MessageView::nextPage, &_messageView));
    else
	addHandledSequence("<PageDown>", std::bind(&MessageView::nextPage, &_messageView));
    if (_generalKeymap.count("previousPage") == 1)
	addHandledSequence(_generalKeymap.find("previousPage")->second, std::bind(&MessageView::previousPage, &_messageView));
    else
	addHandledSequence("<PageUp>", std::bind(&MessageView::previousPage, &_messageView));

    if (_generalKeymap.count("top") == 1)
	addHandledSequence(_generalKeymap.find("top")->second, std::bind(&MessageView::moveToTop, &_messageView));
    else
	addHandledSequence("<Home>", std::bind(&MessageView::moveToTop, &_messageView));
    if (_generalKeymap.count("bottom") == 1)
	addHandledSequence(_generalKeymap.find("bottom")->second, std::bind(&MessageView::moveToTop, &_messageView));
    else
	addHandledSequence("<End>", std::bind(&MessageView::moveToBottom, &_messageView));
    if (_keymap.count("savePart") == 1)
	addHandledSequence(_keymap.find("savePart")->second, std::bind(&MessageView::saveSelectedPart, &_messageView));
    else
	addHandledSequence("<C-s>", std::bind(&MessageView::saveSelectedPart, &_messageView));
    if (_keymap.count("toggleFolding") == 1)
	addHandledSequence(_keymap.find("toggleFolding")->second, std::bind(&MessageView::toggleSelectedPartFolding, &_messageView));
    else
	addHandledSequence("f", std::bind(&MessageView::toggleSelectedPartFolding, &_messageView));

    if (_generalKeymap.count("addTags") == 1)
	addHandledSequence(_generalKeymap.find("addTags")->second, std::bind(&ThreadMessageView::addTags, this));
    else
	addHandledSequence("+", std::bind(&ThreadMessageView::addTags, this));
    if (_generalKeymap.count("removeTags") == 1)
        addHandledSequence(_generalKeymap.find("removeTags")->second , std::bind(&ThreadMessageView::removeTags, this));
    else
	addHandledSequence("-", std::bind(&ThreadMessageView::removeTags, this));

    if (_generalKeymap.count("reply") == 1)
	addHandledSequence(_generalKeymap.find("reply")->second, std::bind(&ThreadView::reply, &_threadView));
    else
	addHandledSequence("r", std::bind(&ThreadView::reply, &_threadView));

    if (_keymap.count("nextMessage") == 1)
	addHandledSequence(_keymap.find("nextMessage")->second, std::bind(&ThreadMessageView::nextMessage, this));
    else
	addHandledSequence("<C-n>",      std::bind(&ThreadMessageView::nextMessage, this));
    if (_keymap.count("previousMessage") == 1)
	addHandledSequence(_keymap.find("previousMessage")->second, std::bind(&ThreadMessageView::previousMessage, this));
    else
	addHandledSequence("<C-p>",      std::bind(&ThreadMessageView::previousMessage, this));
}

ThreadMessageView::~ThreadMessageView()
{
}

void ThreadMessageView::update()
{
    mvhline(threadViewHeight, 0, 0, COLS);

    _threadView.update();
    _messageView.update();
}

void ThreadMessageView::refresh()
{
    _threadView.refresh();
    _messageView.refresh();
}

void ThreadMessageView::resize(const View::Geometry & geometry)
{
    _threadView.resize({ geometry.x, geometry.y, geometry.width, threadViewHeight });
    _messageView.resize({
        geometry.x, threadViewHeight + 1,
        geometry.width, geometry.height - threadViewHeight - 1
    });
}

void ThreadMessageView::nextMessage()
{
    _threadView.next();
    loadSelectedMessage();
    _messageView.moveToTop();
}

void ThreadMessageView::previousMessage()
{
    _threadView.previous();
    loadSelectedMessage();
    _messageView.moveToTop();
}

void ThreadMessageView::loadSelectedMessage()
{
    _messageView.setMessage(_threadView.selectedMessage().id);

    Message message = Notmuch::getMessage(_threadView.selectedMessage().id);
    message.removeTag("unread");
}

std::vector<std::string> ThreadMessageView::status() const
{
    std::vector<std::string> mergedStatus(_threadView.status());
    const std::vector<std::string> & messageViewStatus(_messageView.status());
    std::copy(messageViewStatus.begin(), messageViewStatus.end(), std::back_inserter(mergedStatus));

    return mergedStatus;
}

void ThreadMessageView::addTags()
{
    std::string _messageId = _threadView.selectedMessage().id;
    Message message = Notmuch::getMessage(_messageId);

    try
    {
        std::string tags = StatusBar::instance().prompt("Tags: ", "tags");

        if (!tags.empty()) {
            std::stringstream ss(tags);
            std::string s;

            while (std::getline(ss, s, ' ')) {
                message.addTag(s);
            }

            update();
        }
    }
    catch (const AbortInputException&)
    {
    }
}

void ThreadMessageView::removeTags()
{
    std::string _messageId = _threadView.selectedMessage().id;
    Message message = Notmuch::getMessage(_messageId);

    try
    {
        std::string tags = StatusBar::instance().prompt("Tags: ", "tags");

        if (!tags.empty()) {
            std::stringstream ss(tags);
            std::string s;

            while (std::getline(ss, s, ' ')) {
                message.removeTag(s);
            }

            update();
        }
    }
    catch (const AbortInputException&)
    {
    }
}

// vim: fdm=syntax fo=croql et sw=4 sts=4 ts=8

