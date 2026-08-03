/**
 * @file participants_roster_dialog.hpp
 * @brief Model-scoped dialog to manage judgment participants and groups.
 */

#pragma once

class QWidget;
class Document;

/**
 * @brief Shows the Participants roster dialog (Add / Rename / Remove, plus
 *        a simple Manage groups… sub-dialog). Model-scoped, not app accounts.
 */
void showParticipantsRosterDialog(QWidget* parent, Document* doc);
