#include <fstream>
#include <random>
#include <iostream>

#include "hashMap.h"



using namespace std;
using namespace nlohmann;


static mt19937 gen(chrono::steady_clock::now().time_since_epoch().count());
static uniform_int_distribution<uint32_t> dist(0, 1025);

string generate_id() {
    const auto now = chrono::duration_cast<chrono::milliseconds>(
        chrono::system_clock::now().time_since_epoch()).count();
    const uint32_t random_num = dist(gen);
    return to_string(now) + "_" + to_string(random_num);
}

void insertDoc(HashMap* map, const string& jsonCommand) {
    json doc = json::parse(jsonCommand);
    string id = generate_id();
    doc["_id"] = id;
    map->hashMapInsert(id, doc);

}

bool matchesCondition(const json& doc, const string& field, const json& condition) {
    if (!doc.contains(field)) return false;

    const json& value = doc[field];

    if (!condition.is_object()) {
        return value == condition;
    }

    for (auto& [op, cond_val] : condition.items()) {
        if (op == "$eq") {
            if (value != cond_val) return false;
        }
        else if (op == "$gt") {
            if (!(value.is_number() || value.is_string())) return false;
            if (value <= cond_val) return false;
        }
        else if (op == "$lt") {
            if (!(value.is_number() || value.is_string())) return false;
            if (value >= cond_val) return false;
        }
        else if (op == "$in") {
            if (!cond_val.is_array()) return false;
            bool found = false;
            for (const auto& item : cond_val) {
                if (value == item) { found = true; break; }
            }
            if (!found) return false;
        } else if (op == "$like") {
            if (!value.is_string() || !cond_val.is_string()) return false;
            auto pattern = cond_val.get<string>();
            auto text = value.get<string>();

            size_t pi = 0, ti = 0;
            const size_t textLen = text.size();
            const size_t patternLen = pattern.size();
            int lastMath = -1, lastStar = -1;

            while (ti < textLen) {
                if (pi < patternLen && (text[ti] == pattern[pi] || pattern[pi] == '_')) {
                    ti++;
                    pi++;
                } else if (pi < patternLen && pattern[pi] == '%') {
                    lastStar = pi++;
                    lastMath = ti;
                } else if (lastStar != -1) {
                    ti = ++lastMath;
                    pi = lastStar + 1;
                } else return false;
            }

            while (pi < patternLen && pattern[pi] == '%') pi++;
            return pi == patternLen;
        }
    }
    return true;
}


bool matchesQuery(const json& doc, const json& query) {
    if (query.contains("$and")) {
        for (const auto& cond : query["$and"]) {
            if (!matchesQuery(doc, cond)) return false;
        }
        return true;
    }


    if (query.contains("$or")) {
        for (const auto& cond : query["$or"]) {
            if (matchesQuery(doc, cond)) return true;
        }
        return false;
    }

    // неявный AND
    for (auto& [field, condition] : query.items()) {
        if (field[0] == '$') continue;
        if (!matchesCondition(doc, field, condition)) {
            return false;
        }
    }
    return true;
}


void findDoc(HashMap* map, const string& jsonCommand) {
    json query = json::parse(jsonCommand);
    const auto allItems = map->items();
    bool found = false;

    for (size_t i = 0; i < allItems.size(); i++) {
        if (matchesQuery(allItems[i].second, query)) {
            cout << allItems[i].second.dump(4) << endl;
            found = true;
        }
    }
    if (!found) {
        cout << "No documents found." << endl;
    }
}

void deleteDoc(HashMap* map, const string& jsonCommand) {
    json query = json::parse(jsonCommand);
    const auto allItems = map->items();
    bool deleted = false;

    for (size_t i = 0; i < allItems.size(); i++) {
        if (matchesQuery(allItems[i].second, query)) {
            if (map->deleteById(allItems[i].first)) {
                cout << "Удален документ: " << allItems[i].first << endl;
                deleted = true;
            }
        }
    }
    if (!deleted) {
        cout << "Документы для удаления не найдены." << endl;
    }
}


int main(int argc, char* argv[]) {
    try {
        HashMap map(5);

        string filename, query, jsonCommand;
        if (argc < 4) throw runtime_error ("недостаточно аргументов");

        if (!string(argv[1]).empty()) {
            filename = "/home/dimasik/Рабочий стол/1praka/my_database/"
                                                         + string(argv[1]) + ".json";
        } else {
            throw runtime_error ("имя файла не может быть пустым");
        }

        if (!string(argv[2]).empty()) {
            query = argv[2];
        } else {
            throw runtime_error("запрос пустой или неверный");
        }

        if (!string(argv[3]).empty()) {
            jsonCommand = argv[3];
        } else {
            throw runtime_error("аргумент запроса пустой");
        }

        // Открытие коллекции и загрузка её в hashmap
        json docs = json::array();
        ifstream file(filename);
        if (file.is_open()) {
            try {
                file >> docs;
            } catch (...) {
                cerr << "Файл повреждён — начинаем с нуля." << endl;
            }
            file.close();
        }

        for (auto w : docs) {
            map.hashMapInsert(w["_id"], w);
        }

        if (query == "insert") {
            insertDoc(&map, jsonCommand);
            map.saveToFile(filename);
        } else if (query == "find") {
            findDoc(&map, jsonCommand);
        } else if (query == "delete") {
            deleteDoc(&map, jsonCommand);
            map.saveToFile(filename);
        } else {
            throw runtime_error("неизвестный запрос");
        }



    } catch (const exception& e) {
        cout << "Ошибка: " << e.what() << endl;
    }
    return 0;
}

// ./no_sql_dbms collection insert '{"name": "Alice", "age": 25, "city": "London"}'