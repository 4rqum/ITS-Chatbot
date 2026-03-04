#ifndef KNOWLEDGE_BASE_HPP
#define KNOWLEDGE_BASE_HPP

/*
 * The KnowledgeBase is the bridge that loads all the data within our JSON files at startup,
 * validates it, and gives the rest of the program an easy typed way to access it
 * 
 * Recall that our chatbot is C++ based and has two major parts:
 * 
 * 1. Code/Logic which means take input, tokenize, score intents, ask questions, and give a troubleshooting output
 * 2. Data which is our JSON files: intent.json, questions.json, synonyms.json, and troubleshooting.json
 * 
 * knowledge_base.hpp has 3 jobs:
 * 1. Define the C++ structs that represent our JSON files in memory
 * 2. Loads JSON files from disk into those structs
 * 3. Creates getters so that we can respect the interfaces when we need data in other files.
 * 
 * On top of each function we have a description of what it does and how it may be used.
*/

#include <string>
#include <vector>
#include <unordered_map>

#include <nlohmann/json.hpp>


// represents the thresholds for confidence.
struct Thresholds {
    double min_confidence = 0.35;
    double tie_delta = 0.08;
};

// represent a term and its associated weight.
struct WeightedTerm {
    std::string term;
    double weight;
};

// represents each intent in the knowledge base, with its id, title, description, keywrods, and negative keywrods.
struct Intent {
    std::string id;
    std::string title;
    std::string desc;

    std::unordered_map<std::string, double> keywords;
    std::unordered_map<std::string, double> neg_keywords;
};

// represents each question in our JSON file so that we know what to ask.
struct Question {
    std::string id;
    std::string text;
    std::string type;
    std::vector<std::string> options;
    std::string store_key;
};

// This is our knowledge base of questions that we can ask for general and specific.
struct QuestionsKnowledgeBase {
    std::vector<Question> universal_questions;
    std::unordered_map<std::string, std::vector<Question>> intent_specific_questions;
    bool tie_breaker_enabled = true;
    int max_extra_questions = 2;
};

// This is our knowledge base of synonyms so we know what other keywords may refer to.
struct SynonymsKnowledgeBase {
    std::unordered_map<std::string, std::string> phrase_map;
    std::unordered_map<std::string, std::string> token_map;
};

// Our knowledge base of what to attain from our asked questions.
struct EscalationKnowledgeBase {
    std::string gen_message;
    std::vector<std::string> fields_to_collect;
};

// Will show links for our specific question help.
struct GlobalKnowledgeBase {
    std::unordered_map<std::string, std::string> official_links;
    EscalationKnowledgeBase escalation;
};

// Represents our JSON troubleshooting file so we can correctly represent the steps.
struct TSFlow {
    std::string title;
    std::vector<std::string> steps;
    std::vector<std::string> notes;
    std::vector<std::string> escalate_if;
};

// Represents our troubleshooting for specifc intents.
struct IntentTroubleshooting {
    std::string title;
    std::unordered_map<std::string, TSFlow> flows;
};

// Our troubleshooting for all.
struct TroubleshootingKnowledgeBase {
    GlobalKnowledgeBase global;
    std::unordered_map<std::string, IntentTroubleshooting> intents;
};


class KnowledgeBase {
    public:
        /*
         * 1. Builds file paths like data_dir + "/intents.json"
         * 2. Reads each file
         * 3. Parses each file into the struct representations
         * 4. Returns true if everything loads correctly
         * 5. Returns false if something fails and gives error
        */
        bool loadFromDir(const std::string& data_dir, std::string *err = nullptr);

        // These are getters that return read-only references to the loaded data
        
        // Used by the intent classifier
        const std::vector<Intent> &intents() const;

        // Used by our decision logic
        const Thresholds &thresholds() const;

        // Used by our question asking flow
        const QuestionsKnowledgeBase &questions() const;

        // Used by our normalizer
        const SynonymsKnowledgeBase &synonyms() const;

        // Used in our final output
        const TroubleshootingKnowledgeBase &troubleshooting() const;

    private:

        // opens the file, parses the JSON, catches parse errors, returns false with error if file missing or JSON invalid
        static bool load_json_file(const std::string &path, nlohmann::json* out, std::string *err);

        // will allow us to join paths. Ex: join_path("./data", "intents.json") -> "./data/intents.json"
        static std::string join_path(const std::string &dir, const std::string &file);

        // Takes the JSON object for intents.json and fills out vector and threshold.
        static bool parse_intents(const nlohmann::json &j, std::vector<Intent> *out, Thresholds *thresh, std::string *err);

        // Takes the JSON object for questions.json and fills out question knowledge base.
        static bool parse_questions(const nlohmann::json &j, QuestionsKnowledgeBase *out, std::string *err);

        // Takes the JSON object for synonyms.json and fills out the synonyms knowledge base.
        static bool parse_synonyms(const nlohmann::json &j, SynonymsKnowledgeBase *out, std::string *err);

        // Takes the JSON object for troubleshooting and fills out the troubleshooting knowledege base.
        static bool parse_troubleshooting(const nlohmann::json &j, TroubleshootingKnowledgeBase *out, std::string *err);

        std::vector<Intent> intents_;
        Thresholds thresholds_;
        QuestionsKnowledgeBase questions_;
        SynonymsKnowledgeBase synonyms_;
        TroubleshootingKnowledgeBase troubleshooting_;
};


#endif // KNOWLEDGE_BASE_HPP