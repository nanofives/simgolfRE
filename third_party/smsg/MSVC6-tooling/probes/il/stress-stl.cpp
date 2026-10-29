


#include <vector>
#include <string>
#include <map>

struct Item { int id; std::string name; };

int stress(int n)
{
    std::vector<Item> v;
    std::map<int, std::string> m;
    for (int i = 0; i < n; i++) {
        Item it;
        it.id = i;
        it.name = "item";
        v.push_back(it);
        m[i] = it.name;
    }
    return (int)v.size() + (int)m.size();
}




