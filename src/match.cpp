#include "match.hpp"

#include <algorithm>
#include <unordered_set>

using namespace std;

double Matcher::score_one_intent(const vector<string> &tokens, const Intent &intent) {
    double score = 0.0;

    // we'll use a set to detect if we have ran into duplicate tokens
    unordered_set<string> seen;
    seen.reserve(tokens.size()); // make sure there is enough space for all the tokens

    // go through each token in tokens
    for (const string &t : tokens) { 

        // if the token is just empty, then it doesn't contribute and we move on
        if (t.empty()) {
            continue;
        }

        // if the token has already been seen, then it doesn't contribute and we move on
        if (seen.find(t) != seen.end()) {
            continue;
        }

        // otherwise we insert the token into seen
        seen.insert(t);
        
        // make an iterator to iterate through the keywords in intent and find our token
        auto it_positive = intent.keywords.find(t);

        // once found we'll add to the score using the positive keywords intent score value
        if (it_positive != intent.keywords.end()) {
            score += static_cast<double>(it_positive->second);
        }

        // we'll also do the same with the negative keywords in intent
        auto it_negative = intent.neg_keywords.find(t);

        // once found we'll add the negative score value
        if (it_negative != intent.keywords.end()) {
            score += static_cast<double>(it_negative->second);
        }
    }

    return score; //return score
}

vector<IntentScore> Matcher::score_intents(const vector<string> &tokens, const vector<Intent> &intents) {

    vector<IntentScore> ranked; // make a new vector to hold the intent_id and score for each seperate intent.
    ranked.reserve(intents.size()); // needs to be at least the size of intents incase, we hold each and every intent in our vector

    // go through each intent in intents vector
    for (const Intent &intent : intents) {
        IntentScore s; // for each intent we'll make a struct to hold it's id and score
        s.intent_id = intent.id; // hold id
        s.score = score_one_intent(tokens, intent); // hold score
        ranked.push_back(std::move(s)); // push_back that intentScore into our ranked vector and do this for all intents.
    }

    // once all intents are pushed into our vector, we sort from highest first
    sort(ranked.begin(), ranked.end(), [](const IntentScore &a, const IntentScore &b) {
        if (a.score != b.score) {
            return a.score > b.score;
        }
        return a.intent_id < b.intent_id;
    });

    return ranked; //return the ranked vector with highest score first
}

MatchResult Matcher::decide_intent(const vector<string> &tokens, const vector<Intent> &intents, const Thresholds &thresholds) {
    
    MatchResult result; // create a struct that holds the top intents result
    result.ranked = score_intents(tokens, intents); // result vector ranked will hold our score intents
 
    // if ranked vector is empty, then we aren't confident
    if (result.ranked.empty()) {
        result.below_min_confidence = true;
        return result;
    }

    result.top_intent_id = result.ranked[0].intent_id; // id of top score is id at index 0
    result.top_score = result.ranked[0].score; // score of top score is at index 0

    // if top score is less than the minimum confidence then we aren't confident
    if (result.top_score < thresholds.min_confidence) {
        result.below_min_confidence = true;
    }

    // if result has more than 1 element, then we can check the score for the top second intent
    if (result.ranked.size() >= 2) {
        result.second_intent_id = result.ranked[1].intent_id; // id for top second intent
        result.second_score = result.ranked[1].score; // score for top second intent

        //check for tie_delta
        if ((result.top_score - result.second_score) <= thresholds.tie_delta) {
            result.is_tie = true;
        }
    }

    return result; // return result
}