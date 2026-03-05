#ifndef NLP_HPP
#define NLP_HPP

#include <string>
#include <vector>

#include "knowledge_base.hpp" // We'll use this for SynonymKnowledgeBase

// NLP will be used for normalization and tokenization utilities for the chatbot
/*
 * NLP has a specific set of steps that we want to walk through, a pipeline if you will:
 * 
 * 1. User sends message
 * 2. NLP lowercases entirety of message
 * 3. NLP finds Phrase replacements via the phrase map
 * 4. NLP removes punctuation and normalizes whitespace
 * 5. NLP splits message into tokens via whitesapce
 * 6. NLP finds token replacements via the token_map.
 * 
 * Goal: turn raw user text into canonical tokens that match intents.
*/

class NLP {
    public:

        // Will be used to conduct the whole pipeline
        static std::vector<std::string> tokenize_and_normalize(const std::string &input, const SynonymsKnowledgeBase &synonyms);

        // Can use in Debugging or Testing
        static std::string normalize_text(const std::string &input, const SynonymsKnowledgeBase &synonyms);


    private:

        // Make user input lowercase
        static std::string to_lower_ascii(const std::string &s);

        // Apply phrase map
        static std::string apply_phrase_map(const std::string &s, const SynonymsKnowledgeBase &synonyms);

        // Remove Punctuation
        static std::string strip_and_normalize(const std::string &s);
        
        // Split on Whitespace
        static std::vector<std::string> split(const std::string &s);

        // Apply Token Map
        static void apply_token_map(std::vector<std::string> &tokens, const SynonymsKnowledgeBase &synonyms);

};

#endif