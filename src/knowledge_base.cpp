// For more information about what exactly this does, Check out the HPP file.

#include "knowledge_base.hpp"

#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

using nlohmann::json;

// -------------------------
// Small helpers
// -------------------------

static std::string type_name(const json &j) {
    if (j.is_null()) return "null";
    if (j.is_boolean()) return "boolean";
    if (j.is_number_integer()) return "integer";
    if (j.is_number_float()) return "float";
    if (j.is_number()) return "number";
    if (j.is_string()) return "string";
    if (j.is_array()) return "array";
    if (j.is_object()) return "object";
    return "unknown";
}

template <typename T>
static bool get_required(const json &obj, const char *key, T *out, std::string *err) {
    if (!obj.contains(key)) {
        if (err) *err = std::string("Missing required key: '") + key + "'";
        return false;
    }
    const json &v = obj.at(key);
    try {
        *out = v.get<T>();
        return true;
    } catch (...) {
        if (err) *err = std::string("Key '") + key + "' has wrong type (found " + type_name(v) + ")";
        return false;
    }
}

static bool is_comment_key(const std::string &k) {
    return !k.empty() && k[0] == '_';
}

// -------------------------
// KnowledgeBase Public API
// -------------------------

bool KnowledgeBase::loadFromDir(const std::string &data_dir, std::string *err) {
    const std::string intents_path = join_path(data_dir, "intents.json");
    const std::string questions_path = join_path(data_dir, "questions.json");
    const std::string synonyms_path = join_path(data_dir, "synonyms.json");
    const std::string troubleshooting_path = join_path(data_dir, "troubleshooting.json");

    json j_intents, j_questions, j_synonyms, j_troubleshooting;

    if (!load_json_file(intents_path, &j_intents, err)) return false;
    if (!load_json_file(questions_path, &j_questions, err)) return false;
    if (!load_json_file(synonyms_path, &j_synonyms, err)) return false;
    if (!load_json_file(troubleshooting_path, &j_troubleshooting, err)) return false;

    std::vector<Intent> intents_tmp;
    Thresholds thresholds_tmp;
    QuestionsKnowledgeBase questions_tmp;
    SynonymsKnowledgeBase synonyms_tmp;
    TroubleshootingKnowledgeBase troubleshooting_tmp;

    if (!parse_intents(j_intents, &intents_tmp, &thresholds_tmp, err)) return false;
    if (!parse_questions(j_questions, &questions_tmp, err)) return false;
    if (!parse_synonyms(j_synonyms, &synonyms_tmp, err)) return false;
    if (!parse_troubleshooting(j_troubleshooting, &troubleshooting_tmp, err)) return false;

    intents_ = std::move(intents_tmp);
    thresholds_ = thresholds_tmp;
    questions_ = std::move(questions_tmp);
    synonyms_ = std::move(synonyms_tmp);
    troubleshooting_ = std::move(troubleshooting_tmp);

    return true;
}

const std::vector<Intent>& KnowledgeBase::intents() const { return intents_; }
const Thresholds& KnowledgeBase::thresholds() const { return thresholds_; }
const QuestionsKnowledgeBase& KnowledgeBase::questions() const { return questions_; }
const SynonymsKnowledgeBase& KnowledgeBase::synonyms() const { return synonyms_; }
const TroubleshootingKnowledgeBase& KnowledgeBase::troubleshooting() const { return troubleshooting_; }

// -------------------------
// File loading helpers
// -------------------------

bool KnowledgeBase::load_json_file(const std::string& path, json* out, std::string* err) {
    std::ifstream in(path);
    if (!in.is_open()) {
        if (err) *err = "Failed to open JSON file: " + path;
        return false;
    }
    try {
        in >> *out;
    } catch (const std::exception& e) {
        if (err) *err = "Failed to parse JSON file: " + path + " | " + e.what();
        return false;
    }
    return true;
}

std::string KnowledgeBase::join_path(const std::string& dir, const std::string& file) {
    if (dir.empty()) return file;
    if (dir.back() == '/') return dir + file;
    return dir + "/" + file;
}

// -------------------------
// Parsing: intents.json
// -------------------------

bool KnowledgeBase::parse_intents(const json& j, std::vector<Intent>* out, Thresholds* thresh, std::string* err) {
    if (!j.is_object()) {
        if (err) *err = "intents.json root must be an object";
        return false;
    }

    // thresholds (optional)
    if (j.contains("thresholds")) {
        const json& t = j.at("thresholds");
        if (!t.is_object()) {
            if (err) *err = "intents.json 'thresholds' must be an object";
            return false;
        }
        if (t.contains("min_confidence")) {
            if (!t.at("min_confidence").is_number()) {
                if (err) *err = "thresholds.min_confidence must be a number";
                return false;
            }
            thresh->min_confidence = t.at("min_confidence").get<double>();
        }
        if (t.contains("tie_delta")) {
            if (!t.at("tie_delta").is_number()) {
                if (err) *err = "thresholds.tie_delta must be a number";
                return false;
            }
            thresh->tie_delta = t.at("tie_delta").get<double>();
        }
    }

    if (!j.contains("intents") || !j.at("intents").is_array()) {
        if (err) *err = "intents.json must contain array key 'intents'";
        return false;
    }

    out->clear();
    for (const auto& item : j.at("intents")) {
        if (!item.is_object()) {
            if (err) *err = "Each item in intents[] must be an object";
            return false;
        }

        Intent intent;
        if (!get_required<std::string>(item, "id", &intent.id, err)) return false;
        if (!get_required<std::string>(item, "title", &intent.title, err)) return false;

        // JSON uses "description"
        if (item.contains("description")) {
            if (!item.at("description").is_string()) {
                if (err) *err = "Intent '" + intent.id + "': description must be a string";
                return false;
            }
            intent.desc = item.at("description").get<std::string>();
        } else {
            intent.desc = "";
        }

        // keywords: array of {term, weight}
        intent.keywords.clear();
        if (item.contains("keywords")) {
            if (!item.at("keywords").is_array()) {
                if (err) *err = "Intent '" + intent.id + "': keywords must be an array";
                return false;
            }
            for (const auto& kw : item.at("keywords")) {
                if (!kw.is_object()) {
                    if (err) *err = "Intent '" + intent.id + "': each keyword must be an object";
                    return false;
                }
                std::string term;
                if (!get_required<std::string>(kw, "term", &term, err)) return false;

                if (!kw.contains("weight") || !kw.at("weight").is_number()) {
                    if (err) *err = "Intent '" + intent.id + "': keyword '" + term + "' missing numeric weight";
                    return false;
                }
                const double weight = kw.at("weight").get<double>();
                intent.keywords[term] = weight;
            }
        }

        // negative_keywords
        intent.neg_keywords.clear();
        if (item.contains("negative_keywords")) {
            if (!item.at("negative_keywords").is_array()) {
                if (err) *err = "Intent '" + intent.id + "': negative_keywords must be an array";
                return false;
            }
            for (const auto& kw : item.at("negative_keywords")) {
                if (!kw.is_object()) {
                    if (err) *err = "Intent '" + intent.id + "': each negative_keyword must be an object";
                    return false;
                }
                std::string term;
                if (!get_required<std::string>(kw, "term", &term, err)) return false;

                if (!kw.contains("weight") || !kw.at("weight").is_number()) {
                    if (err) *err = "Intent '" + intent.id + "': negative_keyword '" + term + "' missing numeric weight";
                    return false;
                }
                const double weight = kw.at("weight").get<double>();
                intent.neg_keywords[term] = weight;
            }
        }

        out->push_back(std::move(intent));
    }

    return true;
}

// -------------------------
// Parsing: questions.json
// -------------------------

static bool parse_question_obj(const json& qj, Question* q, std::string* err) {
    if (!qj.is_object()) {
        if (err) *err = "Question must be an object";
        return false;
    }
    if (!get_required<std::string>(qj, "id", &q->id, err)) return false;
    if (!get_required<std::string>(qj, "text", &q->text, err)) return false;
    if (!get_required<std::string>(qj, "type", &q->type, err)) return false;
    if (!get_required<std::string>(qj, "store_key", &q->store_key, err)) return false;

    q->options.clear();
    if (qj.contains("options")) {
        if (!qj.at("options").is_array()) {
            if (err) *err = "Question '" + q->id + "': options must be an array";
            return false;
        }
        for (const auto& opt : qj.at("options")) {
            if (!opt.is_string()) {
                if (err) *err = "Question '" + q->id + "': each option must be a string";
                return false;
            }
            q->options.push_back(opt.get<std::string>());
        }
    }

    return true;
}

bool KnowledgeBase::parse_questions(const json& j, QuestionsKnowledgeBase* out, std::string* err) {
    if (!j.is_object()) {
        if (err) *err = "questions.json root must be an object";
        return false;
    }

    out->universal_questions.clear();
    out->intent_specific_questions.clear();

    if (!j.contains("universal") || !j.at("universal").is_array()) {
        if (err) *err = "questions.json must contain array key 'universal'";
        return false;
    }
    for (const auto& qj : j.at("universal")) {
        Question q;
        if (!parse_question_obj(qj, &q, err)) return false;
        out->universal_questions.push_back(std::move(q));
    }

    if (!j.contains("intent_specific") || !j.at("intent_specific").is_object()) {
        if (err) *err = "questions.json must contain object key 'intent_specific'";
        return false;
    }
    for (auto it = j.at("intent_specific").begin(); it != j.at("intent_specific").end(); ++it) {
        const std::string intent_id = it.key();
        const json& arr = it.value();

        if (!arr.is_array()) {
            if (err) *err = "intent_specific['" + intent_id + "'] must be an array";
            return false;
        }

        std::vector<Question> qs;
        for (const auto& qj : arr) {
            Question q;
            if (!parse_question_obj(qj, &q, err)) return false;
            qs.push_back(std::move(q));
        }
        out->intent_specific_questions[intent_id] = std::move(qs);
    }

    // tie_breaking (optional)
    if (j.contains("tie_breaking")) {
        const json& tb = j.at("tie_breaking");
        if (!tb.is_object()) {
            if (err) *err = "tie_breaking must be an object";
            return false;
        }
        if (tb.contains("enabled")) {
            if (!tb.at("enabled").is_boolean()) {
                if (err) *err = "tie_breaking.enabled must be a boolean";
                return false;
            }
            out->tie_breaker_enabled = tb.at("enabled").get<bool>();
        }
        if (tb.contains("max_extra_questions")) {
            if (!tb.at("max_extra_questions").is_number_integer()) {
                if (err) *err = "tie_breaking.max_extra_questions must be an integer";
                return false;
            }
            out->max_extra_questions = tb.at("max_extra_questions").get<int>();
        }
    }

    return true;
}

// -------------------------
// Parsing: synonyms.json
// -------------------------

bool KnowledgeBase::parse_synonyms(const json& j, SynonymsKnowledgeBase* out, std::string* err) {
    if (!j.is_object()) {
        if (err) *err = "synonyms.json root must be an object";
        return false;
    }

    out->phrase_map.clear();
    out->token_map.clear();

    if (!j.contains("phrase_map") || !j.at("phrase_map").is_object()) {
        if (err) *err = "synonyms.json must contain object key 'phrase_map'";
        return false;
    }
    for (auto it = j.at("phrase_map").begin(); it != j.at("phrase_map").end(); ++it) {
        if (is_comment_key(it.key())) continue;
        if (!it.value().is_string()) {
            if (err) *err = "phrase_map['" + it.key() + "'] must be a string";
            return false;
        }
        out->phrase_map[it.key()] = it.value().get<std::string>();
    }

    if (!j.contains("token_map") || !j.at("token_map").is_object()) {
        if (err) *err = "synonyms.json must contain object key 'token_map'";
        return false;
    }
    for (auto it = j.at("token_map").begin(); it != j.at("token_map").end(); ++it) {
        if (is_comment_key(it.key())) continue;
        if (!it.value().is_string()) {
            if (err) *err = "token_map['" + it.key() + "'] must be a string";
            return false;
        }
        out->token_map[it.key()] = it.value().get<std::string>();
    }

    return true;
}

// -------------------------
// Parsing: troubleshooting.json
// -------------------------

static bool parse_string_array(const json& arr, std::vector<std::string>* out,
                               const std::string& ctx, std::string* err) {
    out->clear();
    if (!arr.is_array()) {
        if (err) *err = ctx + " must be an array";
        return false;
    }
    for (const auto& v : arr) {
        if (!v.is_string()) {
            if (err) *err = ctx + " must contain only strings";
            return false;
        }
        out->push_back(v.get<std::string>());
    }
    return true;
}

bool KnowledgeBase::parse_troubleshooting(const json& j, TroubleshootingKnowledgeBase* out, std::string* err) {
    if (!j.is_object()) {
        if (err) *err = "troubleshooting.json root must be an object";
        return false;
    }

    if (!j.contains("global") || !j.at("global").is_object()) {
        if (err) *err = "troubleshooting.json must contain object key 'global'";
        return false;
    }
    const json& g = j.at("global");

    if (!g.contains("official_links") || !g.at("official_links").is_object()) {
        if (err) *err = "global.official_links must be an object";
        return false;
    }
    out->global.official_links.clear();
    for (auto it = g.at("official_links").begin(); it != g.at("official_links").end(); ++it) {
        if (is_comment_key(it.key())) continue;
        if (!it.value().is_string()) {
            if (err) *err = "official_links['" + it.key() + "'] must be a string";
            return false;
        }
        out->global.official_links[it.key()] = it.value().get<std::string>();
    }

    if (!g.contains("escalation") || !g.at("escalation").is_object()) {
        if (err) *err = "global.escalation must be an object";
        return false;
    }
    const json& esc = g.at("escalation");

    if (!esc.contains("generic_message") || !esc.at("generic_message").is_string()) {
        if (err) *err = "global.escalation.generic_message must be a string";
        return false;
    }
    out->global.escalation.gen_message = esc.at("generic_message").get<std::string>();

    if (!esc.contains("fields_to_collect") || !esc.at("fields_to_collect").is_array()) {
        if (err) *err = "global.escalation.fields_to_collect must be an array";
        return false;
    }
    if (!parse_string_array(esc.at("fields_to_collect"),
                            &out->global.escalation.fields_to_collect,
                            "global.escalation.fields_to_collect", err)) {
        return false;
    }

    if (!j.contains("intents") || !j.at("intents").is_object()) {
        if (err) *err = "troubleshooting.json must contain object key 'intents'";
        return false;
    }

    out->intents.clear();

    for (auto it = j.at("intents").begin(); it != j.at("intents").end(); ++it) {
        const std::string intent_id = it.key();
        const json& intent_obj = it.value();

        if (!intent_obj.is_object()) {
            if (err) *err = "intents['" + intent_id + "'] must be an object";
            return false;
        }

        IntentTroubleshooting itb;

        if (!intent_obj.contains("entry") || !intent_obj.at("entry").is_object()) {
            if (err) *err = "intents['" + intent_id + "'].entry must be an object";
            return false;
        }
        const json& entry = intent_obj.at("entry");
        if (!entry.contains("title") || !entry.at("title").is_string()) {
            if (err) *err = "intents['" + intent_id + "'].entry.title must be a string";
            return false;
        }
        itb.title = entry.at("title").get<std::string>();

        if (!intent_obj.contains("flows") || !intent_obj.at("flows").is_object()) {
            if (err) *err = "intents['" + intent_id + "'].flows must be an object";
            return false;
        }

        itb.flows.clear();
        const json& flows = intent_obj.at("flows");

        for (auto fit = flows.begin(); fit != flows.end(); ++fit) {
            const std::string flow_id = fit.key();
            const json& fobj = fit.value();

            if (!fobj.is_object()) {
                if (err) *err = "flows['" + flow_id + "'] under intent '" + intent_id + "' must be an object";
                return false;
            }

            TSFlow flow;
            if (!fobj.contains("title") || !fobj.at("title").is_string()) {
                if (err) *err = "Intent '" + intent_id + "' flow '" + flow_id + "': missing string title";
                return false;
            }
            flow.title = fobj.at("title").get<std::string>();

            if (!fobj.contains("steps")) {
                if (err) *err = "Intent '" + intent_id + "' flow '" + flow_id + "': missing steps";
                return false;
            }
            if (!parse_string_array(fobj.at("steps"), &flow.steps,
                                    "Intent '" + intent_id + "' flow '" + flow_id + "'.steps", err)) {
                return false;
            }

            flow.notes.clear();
            if (fobj.contains("notes")) {
                if (!parse_string_array(fobj.at("notes"), &flow.notes,
                                        "Intent '" + intent_id + "' flow '" + flow_id + "'.notes", err)) {
                    return false;
                }
            }

            flow.escalate_if.clear();
            if (fobj.contains("escalate_if")) {
                if (!parse_string_array(fobj.at("escalate_if"), &flow.escalate_if,
                                        "Intent '" + intent_id + "' flow '" + flow_id + "'.escalate_if", err)) {
                    return false;
                }
            }

            itb.flows[flow_id] = std::move(flow);
        }

        out->intents[intent_id] = std::move(itb);
    }

    return true;
}