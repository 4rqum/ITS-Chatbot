#include "nlp.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

using namespace std;

// Public API

// This is our pipeline
// We first normalize the input, turning it into a clean string
// Then we split the normalized input into valid tokens (words)
// Then we apply the token map which normalized each word to it's valid type of issue
// Finally we remove any empty tokens and return the tokens
vector<string> NLP::tokenize_and_normalize(const string &input, const SynonymsKnowledgeBase &synonyms) {

    const string normalized = normalize_text(input, synonyms);
    vector<string> tokens = split(normalized);
    apply_token_map(tokens, synonyms);
    tokens.erase(remove_if(tokens.begin(), tokens.end(), [](const std::string& t) 
        { return t.empty(); }),
        tokens.end()
    );
    return tokens;
}

// This is part of the pipeline so we can use it when we tokenize
// First we convert the raw input to lowercase so we have no issues when apply the phrase map and finding synonyms.
// We then replace the multi-word phrases by applying the phrase map (ex: sign in -> signin)
// Finally we strip all punctuation and normalize the whitespace in between
string NLP::normalize_text(const string &input, const SynonymsKnowledgeBase &synonyms) {

    string s = to_lower_ascii(input);
    s = apply_phrase_map(s, synonyms);
    s = strip_and_normalize(s);
    return s;
}

// Private Helpers:
string NLP::to_lower_ascii(const string &s) {
    string out; // make a new empty string, recall a string is just an array of chars
    out.reserve(s.size()); // since it's an array we'll make our size of our new string the same as the size of our user input.

    //go through each character in the input string and put it's lowercase counterpart in our new string.
    for (unsigned char ch : s) {
        out.push_back(static_cast<char>(tolower(ch)));
    }

    return out; //return lowercased string
}

// We'll use this helper to replace all synonyms using phrase map.
static void replace_all(string &s, const string &from, const string &to) {

    // if the substring 'from' is empty then we just return
    if (from.empty()) {
        return;
    }

    // else go through all the strings and and replace every occurrence of substring 'from' with 'to'.
    size_t position = 0;
    while ((position = s.find(from, position)) != string::npos) {
        s.replace(position, from.size(), to);
        position += to.size();
    }
}

string NLP::apply_phrase_map(const string &s, const SynonymsKnowledgeBase &synonyms) {
    vector<pair<string, string>> pairs; // declare vector of pairs
    pairs.reserve(synonyms.phrase_map.size()); // make space for the phrase map

    //go through all key value pairs in the phrase map and push back the key value pairs into the vector
    for (const auto &kv : synonyms.phrase_map) {
        pairs.push_back(kv);
    }

    // we sort by longest phrase first because we don't want to fire early and the string in a way that breaks the longer match
    sort(pairs.begin(), pairs.end(), [](const auto& a, const auto& b) { 
        return a.first.size() > b.first.size(); 
    });


    string out = s; // declare out with string s

    // go through all the key value pairs in our pairs vector and assign the key as 'from' and value as 'to'
    for (const auto &kv : pairs) {
        const string &from = kv.first;
        const string &to = kv.second;
        replace_all(out, from, to); // replace all instance of 'from' with 'to' and put them in our string out 
    }

    return out; //return out
}

string NLP::strip_and_normalize(const string &s) {

    string tmp; // new string temp
    tmp.reserve(s.size()); //make space for temo with string s

    // go through all characters in s and if character is alphanumeric then pushback the character into tmp else push_back one space. So we normalize by removing punctuation
    for (unsigned char ch : s) { 
        if (isalnum(ch)) {
            tmp.push_back(static_cast<char>(ch));
        } else {
            tmp.push_back(' ');
        }
    }

    string out; //new string out
    out.reserve(tmp.size()); // we reserve the size of out via tmp

    bool in_space = true; 

    // go through all characters in tmp and if the character is a space then collapse it into a single space
    for (unsigned char ch : tmp) {
        if (isspace(ch)) {
            if (!in_space) {
                out.push_back(' ');
                in_space = true;
            }
        } else {
            // else we push back the normal character.
            out.push_back(static_cast<char>(ch));
            in_space = false;
        }
    }

    // trim trailing space
    if (!out.empty() && out.back() == ' ') {
        out.pop_back();
    }

    return out; //return out
}

vector<string> NLP::split(const string &s) {

    vector<string> tokens;
    string curr;

    /*
     * basically we are using the else statement more here
     * we push back the char into curr until we run into a white space
     * Once we run into a white space, then curr isn't empty and we push it back into the tokens vector
     * Now tokens has one word in it and we clear it and do it again.
    */

    // go through all char in s, only push curr when there is no content
    for (unsigned char ch : s) {
        if (isspace(ch)) {
            if (!curr.empty()) {
                tokens.push_back(curr); //push back curr if no content
                curr.clear(); //clear curr out
            }
        } else {
            // else push back the the char in s
            curr.push_back(static_cast<char>(ch));
        }
    }

    if (!curr.empty()) {
        tokens.push_back(curr);
    }

    return tokens; //return tokens vector
}

static string normalize_token(const string &token, const SynonymsKnowledgeBase &synonyms) {

    /*
     * go through token map and find the token
     * if the iterator isn't at the end of the token map, we just return the value part of that token.
     * so ex: authenticating -> authenticate
    */

    auto it = synonyms.token_map.find(token);
    if (it != synonyms.token_map.end()) {
        return it->second;
    }

    return token; //return the token
}

void NLP::apply_token_map(vector<string> &tokens, const SynonymsKnowledgeBase &synonyms) {

    // go through each token in token, normalize it via the synonyms.
    for (string &t : tokens) {
        t = normalize_token(t, synonyms);
    }
}
