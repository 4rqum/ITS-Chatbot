#ifndef MATCH_HPP
#define MATCH_HPP

#include <string>
#include <vector>

#include "knowledge_base.hpp"

/*
 * So at this stage, we have already done the nlp and now our user text was normalized and turned into tokens
 * By tokens, I mean seperate keywords. 
 * The Matcher should use those tokens and tell us:
 * 1. Which intent does this look like the most? (Okta VS VPN VS MPrint)
 * 2. How confident are we?
 * 3. Is it too close to call between two intents? (tie_delta threshold)
*/

// struct to hold a specific intent and it's score
struct IntentScore {
    std::string intent_id;
    double score = 0.0;
};

// struct to hold final decision after scoring
struct MatchResult {
    std::string top_intent_id;
    double top_score = 0.0;

    bool is_tie = false; // true if top two are within tie_delta
    std::string second_intent_id; //valid iff we have a tie in intents.
    double second_score = 0.0;

    bool below_min_confidence = false; //true if top_score is < min_confidence

    std::vector<IntentScore> ranked; //sorted descending by score
};

class Matcher {
    public:
        // Score all intents and return sorted result in descending order
        static std::vector<IntentScore> score_intents(const std::vector<std::string> &tokens, const std::vector<Intent> &intents);

        // score and apply thresholds
        static MatchResult decide_intent(const std::vector<std::string> &tokens, const std::vector<Intent> &intents, const Thresholds &thresholds);
    
    private:
        // score a singular intent
        static double score_one_intent(const std::vector<std::string> &tokens, const Intent &intent);
};       



#endif