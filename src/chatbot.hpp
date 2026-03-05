#ifndef CHATBOT_HPP
#define CHATBOT_HPP

#include <string>
#include <vector>

#include "knowledge_base.hpp"
#include "conversation_state.hpp"
#include "match.hpp"
#include "nlp.hpp"

/*
 * This file will be our chat bot
 * Chatbot will be responsible for knowing what to ask after each step
 * After each step, the Chatbot will either ask another question
 * OR
 * output a final steps list.
*/

// This struct will hold the active intent id and match, what questions to ask, what flow to ouput, and escalation message
struct BotResponse {
    bool needs_more_info = false;

    std::string active_intent_id;
    MatchResult match;

    std::vector<Question> questions_to_ask;

    // final troubleshooting output (when needs_more_info = false)
    std::string flow_title;
    std::vector<std::string> final_steps;
    std::vector<std::string> final_notes;
    std::vector<std::string> final_escalation;

    // Escalation message
    bool suggest_escalation = false;
    std::string escalation_message;
    std::vector<std::string> escalation_fields_to_collect;

    // if we can't classify
    bool below_min_confidence = false;
};

class Chatbot {
    public: 

        // Load all JSON files via KnowledgeBase
        bool init(const std::string &data_dir, std::string *err = nullptr);

        // Reset Conversation Memory
        void start_new_session();

        // User types a msg, we tokenize and score intents and decide next step
        BotResponse process_user_message(const std::string &user_text);

        // Used to store answers 
        BotResponse submit_answer(const std::string &question_id, const std::string &answer_value);

    private:

        KnowledgeBase kb_;
        ConversationState state_;
        bool initialized_ = false;

        // Decide which questions should be asked next
        std::vector<Question> pick_next_question();

        // Decide which troubleshooting flow to choose based on intent and answers.
        std::string choose_flow_id(const std::string &intent_id) const;

        // Build final response from troubleshooting flow
        BotResponse build_final_response(const std::string &intent_id, const std::string &flow_id) const;

        // Map question_id to store_key and store in state answers
        bool store_answer_by_question_id(const std::string &question_id, const std::string &answer_value);


};

#endif