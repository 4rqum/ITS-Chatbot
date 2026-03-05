#include <iostream>
#include <string>
#include <vector>

#include "chatbot.hpp"

using namespace std;

// static void print_match_debug(const MatchResult &m) {
//     cout << "\n[debug] top_intent=" << m.top_intent_id
//          << " score=" << m.top_score
//          << " tie=" << (m.is_tie ? "true" : "false")
//          << " below_min_conf=" << (m.below_min_confidence ? "true" : "false")
//          << "\n";

//     if (!m.ranked.empty()) {
//         cout << "[debug] ranked:\n";
//         for (const auto &r : m.ranked) {
//             cout << "  - " << r.intent_id << ": " << r.score << "\n";
//         }
//     }
// }

static void print_final_response(const BotResponse &resp) {
    cout << "\n=== " << (resp.flow_title.empty() ? "Troubleshooting Steps" : resp.flow_title) << " ===\n";

    if (!resp.final_steps.empty()) {
        cout << "\nSteps:\n";
        for (size_t i = 0; i < resp.final_steps.size(); i++) {
            cout << (i + 1) << ". " << resp.final_steps[i] << "\n";
        }
    }

    if (!resp.final_notes.empty()) {
        cout << "\nNotes:\n";
        for (const auto &n : resp.final_notes) {
            cout << "- " << n << "\n";
        }
    }

    // These are "escalate_if" strings from the flow (conditions/hints)
    if (!resp.final_escalation.empty()) {
        cout << "\nEscalate if:\n";
        for (const auto &e : resp.final_escalation) {
            cout << "- " << e << "\n";
        }
    }

    // Generic escalation message (from troubleshooting.json global)
    if (resp.suggest_escalation) {
        cout << "\n" << resp.escalation_message << "\n";
        if (!resp.escalation_fields_to_collect.empty()) {
            cout << "Include:\n";
            for (const auto &f : resp.escalation_fields_to_collect) {
                cout << "- " << f << "\n";
            }
        }
    }

    cout << "\n=============================\n\n";
}

static void print_question(const Question &q) {
    cout << "\n[Question] " << q.text << "\n";

    // If options exist, print them 
    if (!q.options.empty()) {
        for (size_t i = 0; i < q.options.size(); i++) {
            cout << "  " << (i + 1) << ") " << q.options[i] << "\n";
        }
        cout << "Type the option number (or type your own text): ";
    } else {
        cout << "Your answer: ";
    }
}

static string read_line_trimmed() {
    string s;
    getline(cin, s);

    // trim simple whitespace
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ' || s.back() == '\t')) {
        s.pop_back();
    }
    size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\t')) {
        start++;
    }
    if (start > 0) s = s.substr(start);

    return s;
}

static string resolve_answer_value(const Question &q, const string &raw) {
    // If user typed a number that matches an option, return the option text.
    // Otherwise return raw text as-is.
    if (!q.options.empty()) {
        bool all_digits = !raw.empty();
        for (char c : raw) {
            if (c < '0' || c > '9') {
                all_digits = false;
                break;
            }
        }
        if (all_digits) {
            int idx = stoi(raw);
            if (idx >= 1 && idx <= (int)q.options.size()) {
                return q.options[(size_t)(idx - 1)];
            }
        }
    }
    return raw;
}

int main(int argc, char **argv) {
    // Default data directory 
    // Example: ./data or ./src/data depending on where your JSON lives
    string data_dir = "./data";
    if (argc >= 2) {
        data_dir = argv[1];
    }

    Chatbot bot;
    string err;
    if (!bot.init(data_dir, &err)) {
        cerr << "Failed to initialize chatbot: " << err << "\n";
        cerr << "Tried data_dir: " << data_dir << "\n";
        return 1;
    }

    cout << "UMich ITS Chatbot (CLI)\n";
    cout << "Data dir: " << data_dir << "\n";
    cout << "Type 'exit' to quit, 'reset' to start a new session.\n\n";

    while (true) {
        cout << "You: ";
        string user_text = read_line_trimmed();

        if (!cin) break;
        if (user_text == "exit" || user_text == "quit") break;

        if (user_text == "reset") {
            bot.start_new_session();
            cout << "Session reset.\n\n";
            continue;
        }

        BotResponse resp = bot.process_user_message(user_text);

        // print_match_debug(resp.match);

        // If bot asks questions, handle them interactively until done or escalation
        while (resp.needs_more_info && !resp.questions_to_ask.empty()) {
            const Question &q = resp.questions_to_ask.front();

            print_question(q);
            string raw_answer = read_line_trimmed();
            if (!cin) return 0;

            if (raw_answer == "exit" || raw_answer == "quit") return 0;

            // Convert "2" -> option text if applicable
            string answer_value = resolve_answer_value(q, raw_answer);

            resp = bot.submit_answer(q.id, answer_value);

            // print_match_debug(resp.match);
        }

        // If we ended with escalation only, show escalation message
        if (resp.suggest_escalation && resp.final_steps.empty() && resp.flow_title.empty()) {
            cout << "\n" << resp.escalation_message << "\n";
            if (!resp.escalation_fields_to_collect.empty()) {
                cout << "Include:\n";
                for (const auto &f : resp.escalation_fields_to_collect) {
                    cout << "- " << f << "\n";
                }
            }
            cout << "\n";
            continue;
        }

        // Otherwise, print final response
        print_final_response(resp);
    }

    cout << "Goodbye.\n";
    return 0;
}