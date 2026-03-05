#ifndef CONVERSATION_STATE_HPP
#define CONVERSATION_STATE_HPP

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "match.hpp"

/*
 * This HPP file is basically the memory of our chat session:
 * 
 * 1. We'll store the user answers via the store key from questions.json
 * 2. We'll store what we already asked so we avoid repetition
 * 3. We'll store the current best guess intent + match result
 * 4. We'll store the troubleshooting flow we chose once we choose it
 * 5. We'll store the escalation fields
*/

struct ConversationState {
    
    // store the raw user messages
    std::vector<std::string> user_messages; 

    // store canonical tokens from the most recent user message
    std::vector<std::string> last_token;

    // store answers collected from diagnostic questions
    // key = questions.store_key
    // value = the selected option or free response
    std::unordered_map<std::string, std::string> answers;

    // we'll store and track which questions have already been asked
    std::unordered_set<std::string> asked_question_ids;

    // we'll store the current intent ranking decision
    MatchResult last_match;

    // We'll store the actively decided intent
    std::string active_intent_id;

    // We'll store our chosen troubleshooting flow
    std::string chosen_flow_id;

    // We'll store whether we need escalation or not
    bool needs_escalation = false;

    // We'll store final response steps
    std::vector<std::string> final_steps;
    std::vector<std::string> final_notes;
    std::vector<std::string> final_escalation;

    // helpers:

    bool has_answer(const std::string &store_key) const {
        return answers.find(store_key) != answers.end();
    }

    const std::string *get_answer_ptr(const std::string &store_key) const {
        auto it = answers.find(store_key);

        if (it == answers.end()) {
            return nullptr;
        }

        return &it->second;
    }

    void set_answer(const std::string &store_key, const std::string &value) {
        answers[store_key] = value;
    }

    bool already_asked(const std::string &questions_id) {
        return asked_question_ids.find(questions_id) != asked_question_ids.end();
    }

    void mark_asked(const std::string &question_id) {
        asked_question_ids.insert(question_id);
    }

    void reset_for_new_session() {
        user_messages.clear();
        last_token.clear();
        answers.clear();
        asked_question_ids.clear();
        last_match = MatchResult{};
        active_intent_id.clear();
        chosen_flow_id.clear();
        needs_escalation = false;
        final_steps.clear();
        final_notes.clear();
        final_escalation.clear();      
    }
};

#endif