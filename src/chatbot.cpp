#include "chatbot.hpp"
#include <algorithm>

using namespace std;

bool Chatbot::init(const string &data_dir, string *err) {

    // return false if JSON failed to load
    if (!kb_.loadFromDir(data_dir, err)) {
        return false;
    }

    // else we loaded correctly, and we can start a new session
    initialized_ = true;
    start_new_session();

    return true;
}

void Chatbot::start_new_session() {

    //reset state onto the new session
    state_.reset_for_new_session();
}

BotResponse Chatbot::process_user_message(const string &user_text) {

    BotResponse resp;

    // Escalate an error / notification that JSON files weren't loaded
    if (!initialized_) {
        resp.suggest_escalation = true;
        resp.escalation_message = "Chatbot not initialized (KnowledgeBase not loaded).";
        return resp;
    }

    // Pushback user text into our vector of strings (users question/complaint)
    state_.user_messages.push_back(user_text);

    // 1) Use NLP to tokenize and normalize the user message
    state_.last_token = NLP::tokenize_and_normalize(user_text, kb_.synonyms());

    // 2) Match intents
    state_.last_match = Matcher::decide_intent(state_.last_token, kb_.intents(), kb_.thresholds());
    resp.match = state_.last_match;
    resp.active_intent_id = state_.last_match.top_intent_id;
    resp.below_min_confidence = state_.last_match.below_min_confidence;

    // if we can't confidently pick an intent, we should ask more questions or escalate
    if (state_.last_match.below_min_confidence) {
        auto qs = pick_next_question();
        if (!qs.empty()) {
            resp.needs_more_info = true;
            resp.questions_to_ask = std::move(qs);
            return resp;
        }

        resp.suggest_escalation = true;
        resp.escalation_message = kb_.troubleshooting().global.escalation.gen_message;
        resp.escalation_fields_to_collect = kb_.troubleshooting().global.escalation.fields_to_collect;
        return resp;
    }

    // Set active intent
    state_.active_intent_id = state_.last_match.top_intent_id;

    // 3) Ask questions if needed
    auto questions = pick_next_question();
    if (!questions.empty()) {
        resp.needs_more_info = true;
        resp.questions_to_ask = std::move(questions);
        return resp;
    }

    // 4) Otherwise choose a flow and output steps
    const string flow_id = choose_flow_id(state_.active_intent_id);
    return build_final_response(state_.active_intent_id, flow_id);
}

BotResponse Chatbot::submit_answer(const string &question_id, const string &answer_value) {
    BotResponse resp;

    if (!store_answer_by_question_id(question_id, answer_value)) {
        resp.suggest_escalation = true;
        resp.escalation_message = "Internal error: unknown question_id '" + question_id + "'";
        return resp;
    }

    state_.mark_asked(question_id);

    // After we store, we must decide what's next: final troubleshooting or more questions
    auto questions = pick_next_question();
    resp.match = state_.last_match;
    resp.active_intent_id = state_.active_intent_id;

    if (!questions.empty()) {
        resp.needs_more_info = true;
        resp.questions_to_ask = std::move(questions);
        return resp;
    }

    const string flow_id = choose_flow_id(state_.active_intent_id);
    return build_final_response(state_.active_intent_id, flow_id);
}

vector<Question> Chatbot::pick_next_question() {
    vector<Question> out;

    const auto &qkb = kb_.questions();

    const int kMaxUniversal = 4;
    const int kMaxIntentSpecific = 2;


    // Count how many universal questions have been asked
    int universal_asked = 0;
    for (const auto &q : qkb.universal_questions) {
        if (state_.already_asked(q.id)) {
            universal_asked++;
        }
    }

    // Ask remaining universal questions first 
    if (universal_asked < kMaxUniversal) {
        for (const auto &q : qkb.universal_questions) {
            if (!state_.already_asked(q.id)) {
                out.push_back(q);
                return out; // ask 1 at a time
            }
        }
    }

    // If we don't have an active intent yet, we can't ask intent-specific
    if (state_.active_intent_id.empty()) {
        return out;
    }


    // Count how many intent specific questions have been asked 
    auto is_intent_specific_qid = [&](const string &qid) -> bool {
        for (const auto &pair : qkb.intent_specific_questions) {
            const auto &vec = pair.second;
            for (const auto &q : vec) {
                if (q.id == qid) return true;
            }
        }
        return false;
    };

    int intent_specific_asked = 0;
    for (const auto &qid : state_.asked_question_ids) {
        if (is_intent_specific_qid(qid)) {
            intent_specific_asked++;
        }
    }

    if (intent_specific_asked >= kMaxIntentSpecific) {
        return out; // done asking questions
    }


    // Vibed Out Helper: pick next unasked intent-specific question for a given intent
    auto pick_one_for_intent = [&](const string &intent_id) -> bool {
        auto it = qkb.intent_specific_questions.find(intent_id);
        if (it == qkb.intent_specific_questions.end()) return false;

        for (const auto &q : it->second) {
            if (!state_.already_asked(q.id)) {
                out.push_back(q);
                return true;
            }
        }
        return false;
    };

    // Tie-breaking policy:
    // If top two intents are close, ask 1 from each 
    if (qkb.tie_breaker_enabled && state_.last_match.is_tie &&
        !state_.last_match.second_intent_id.empty()) {

        // ask one from top intent
        if (intent_specific_asked < kMaxIntentSpecific) {
            if (pick_one_for_intent(state_.last_match.top_intent_id)) {
                return out; // ask 1 at a time
            }
        }

        // ask one from second intent
        if (intent_specific_asked < kMaxIntentSpecific) {
            if (pick_one_for_intent(state_.last_match.second_intent_id)) {
                return out;
            }
        }

        return out;
    }

    pick_one_for_intent(state_.active_intent_id);
    return out;
}

// Select troubleshooting flow
string Chatbot::choose_flow_id(const string &intent_id) const {
    const auto &ts = kb_.troubleshooting();

    auto it = ts.intents.find(intent_id);
    if (it == ts.intents.end()) {
        return "";
    }

    const auto &flows = it->second.flows;
    if (flows.empty()) {
        return "";
    }

    auto def = flows.find("default");
    if (def != flows.end()) {
        return "default";
    }

    return flows.begin()->first;
}

// Build final response
BotResponse Chatbot::build_final_response(const string &intent_id, const string &flow_id) const {
    BotResponse resp;

    resp.needs_more_info = false;
    resp.active_intent_id = intent_id;
    resp.match = state_.last_match;
    resp.below_min_confidence = state_.last_match.below_min_confidence;

    const auto &tb = kb_.troubleshooting();

    auto it_intent = tb.intents.find(intent_id);
    if (it_intent == tb.intents.end()) {
        resp.suggest_escalation = true;
        resp.escalation_message = tb.global.escalation.gen_message;
        resp.escalation_fields_to_collect = tb.global.escalation.fields_to_collect;
        return resp;
    }

    const auto &flows = it_intent->second.flows;
    auto itFlow = flows.find(flow_id);

    // if flow_id not found, pick default/first
    if (itFlow == flows.end()) {
        string fallback = "";
        auto def = flows.find("default");
        if (def != flows.end()) fallback = "default";
        else if (!flows.empty()) fallback = flows.begin()->first;

        if (fallback.empty()) {
            resp.suggest_escalation = true;
            resp.escalation_message = tb.global.escalation.gen_message;
            resp.escalation_fields_to_collect = tb.global.escalation.fields_to_collect;
            return resp;
        }
        itFlow = flows.find(fallback);
    }

    const TSFlow &flow = itFlow->second;

    resp.flow_title = flow.title;
    resp.final_steps = flow.steps;
    resp.final_notes = flow.notes;
    resp.final_escalation = flow.escalate_if;

    // Simple escalation rule:
    // If the flow has any "escalate" entries, we treat that as a hint to suggest escalation.
    if (!flow.escalate_if.empty()) {
        resp.suggest_escalation = true;
        resp.escalation_message = tb.global.escalation.gen_message;
        resp.escalation_fields_to_collect = tb.global.escalation.fields_to_collect;
    }

    return resp;
}

// Store an answer by question_id
bool Chatbot::store_answer_by_question_id(const string &question_id, const string &answer_value) {
    const auto &qkb = kb_.questions();

    auto store_from_question = [&](const Question &q) -> bool {
        if (q.store_key.empty()) return false;
        state_.set_answer(q.store_key, answer_value);
        return true;
    };

    // search universal questions
    for (const auto &q : qkb.universal_questions) {
        if (q.id == question_id) {
            return store_from_question(q);
        }
    }

    // search all intent-specific questions
    for (const auto &pair : qkb.intent_specific_questions) {
        const auto &vec = pair.second;
        for (const auto &q : vec) {
            if (q.id == question_id) {
                return store_from_question(q);
            }
        }
    }

    return false;
}