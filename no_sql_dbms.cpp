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
    cout << "Document inserted successfully." << endl;
}

void help() {
    cout << "./no_sql_dbm -h, --help    показать эту справку" << endl;
    cout << "./no_sql_dbm <filename> <command> <query> " << endl;
    cout << "=========filename==============" << endl;
    cout
    << "Файл автоматически сохраняется с разрешением .json по пути /директория проекта/my_database/имя вашего файла.json "
            << endl;
    cout << "=========command===============" << endl;
    cout << "Перечень доступных команд: " << endl;
    cout << "insert - добавить документ в коллекцию " << endl;
    cout << "find   - поиск документа по параметрам " << endl;
    cout << "delete - удаление документа по параметрам " << endl;
    cout << "=========query=================" << endl;
    cout << "Доступные операторы и примеры их использования в поиске и в удалении: " << endl;
    cout << R"({"$eq": [{"_id": "1763888722455_1021"}]}     [$eq - равенство (используется по умолчанию)])" << endl;
    cout << "{\"name\": \"Alice\", \"city\": \"London\"}        [неявный AND]" << endl;
    cout << "{\"$and\": [{\"age\": 25}, {\"city\": \"Paris\"}]} [явный $AND]" << endl;
    cout << R"('{"$or": [{"age": 25}, {"city": "Paris"}]}  [$or]')" << endl;
    cout << "{\"age\": {\"$gt\": 20}}                           [$gt - больше]" << endl;
    cout << "{\"age\": {\"$lt\": 20}}                           [$gt - меньше]" << endl;
    cout << "{\"name\": {\"$like\": \"Ali%\"}}                  [$like - поиск по маске строки (% - любая строка, _ - один символ)]" << endl;
    cout << "{\"city\": {\"$in\": [\"London\", \"Paris\"]}}     [$in - проверить принадлежность к массиву]" << endl;
    cout << "=========Примеры использования=========" << endl;
    cout << "./no_sql_dbms collection insert \'{\"name\": \"Alice\", \"age\": 25, \"city\": \"London\"}\'" << endl;
    cout << "Вставка документа со случайным сгенерированным id" << endl;
    cout << "./no_sql_dbms collection find \'{\"$or\": [{\"age\": 25}, {\"city\": \"Paris\"}]}\'" << endl;
    cout << "Поиск документа с оператором $or" << endl;
    cout << "./no_sql_dbms collection delete \'{\"name\": {\"$like\": \"A%\"}}\' " << endl;
    cout << "Удаление документа с использованием оператора $like" << endl;
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
            const auto pattern = cond_val.get<string>();
            const auto text = value.get<string>();

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


void findDoc(const HashMap* map, const string& jsonCommand) {
    const json query = json::parse(jsonCommand);
    const auto allItems = map->items();
    bool found = false;

    for (const auto& [fst, snd] : allItems) {
        if (matchesQuery(snd, query)) {
            cout << snd.dump(4) << endl;
            found = true;
        }
    }
    if (!found) {
        cout << "No documents found." << endl;
    }
}

void deleteDoc(HashMap* map, const string& jsonCommand) {
    const json query = json::parse(jsonCommand);
    const auto allItems = map->items();
    bool deleted = false;

    for (const auto &[fst, snd] : allItems) {
        if (matchesQuery(snd, query)) {
            if (map->deleteById(fst)) {
                cout << "Удален документ: " << fst << endl;
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
        if (argc == 1 || (argc == 2 && (string(argv[1]) == "-h" || string(argv[1]) == "--help"))) {
            help();
            return 0;

        }
        string collection_name = argv[1];
        string filename = "my_database/" + collection_name + ".json";
        std::filesystem::create_directories("my_database");

        HashMap map(3);

        // Открытие коллекции и загрузка её в hashmap
        json docs = json::array();
        if (ifstream file(filename); file.is_open()) {
            try {
                file >> docs;
            } catch (...) {
                cerr << "Файл повреждён — начинаем с нуля." << endl;
            }
            file.close();
        }

        for (const auto& doc : docs) {
            string id = doc["_id"];
            map.hashMapInsert(id, doc);
        }

        if (argc == 3) {
            if (string(argv[2]).empty()) throw runtime_error("пустой аргумент запроса");
            string command = argv[2];
            if (command == "print") {
                cout << "=== Коллекция: " << collection_name << " ===" << endl;
                map.print();
                return 0;
            } else {
                if (command == "find" || command == "insert" || command == "delete") {
                    throw runtime_error("команда '" + command + "' требует json-запрос");
                } else {
                    throw runtime_error("неизвестная команда");
                }
            }
        }

        if (argc != 4) {
            throw runtime_error("ожидается: <коллекция> <команда> <json>");
        }
        if (string(argv[2]).empty()) throw runtime_error("пустой аргумент запроса");
        if (string(argv[3]).empty()) throw runtime_error("пустой аргумент json запроса");
        string command = argv[2];
        string jsonCommand = argv[3];

        if (command == "insert") {
            insertDoc(&map, jsonCommand);
            map.saveToFile(filename);
        } else if (command == "find") {
            findDoc(&map, jsonCommand);
        } else if (command == "delete") {
            deleteDoc(&map, jsonCommand);
            map.saveToFile(filename);
        } else if (command == "print"){
            map.print();
        }else {
            throw runtime_error("неизвестный запрос, введите ключ -h, --help для справки");
        }



    } catch (const exception& e) {
        cout << "Ошибка: " << e.what() << endl;
    }
    return 0;
}

// ./no_sql_dbms collection insert '{"name": "Alice", "age": 25, "city": "London"}'