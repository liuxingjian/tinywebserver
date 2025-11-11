// /home/kali/workplace/cpp/mysimpleWebServer/test/test1.cpp
// 力扣: 环形链表 II (找出环的起点)
// 使用哈希表检测

#include <iostream>
#include <vector>
#include <unordered_set>
using namespace std;

struct ListNode {
    int val;
    ListNode *next;
    ListNode(int x): val(x), next(nullptr) {}
};

// 哈希表方法：保存已访问节点，首次再次访问的节点即为环的起点
ListNode* detectCycle(ListNode* head) {
    unordered_set<ListNode*> seen;
    ListNode* cur = head;
    while (cur) {
        if (seen.count(cur)) return cur;
        seen.insert(cur);
        cur = cur->next;
    }
    return nullptr;
}

// 安全的 indexOf：若发现循环且未找到 target，则返回 -1，避免无限循环
int indexOf(ListNode* head, ListNode* target) {
    if (!target) return -1;
    unordered_set<ListNode*> seen;
    int idx = 0;
    while (head) {
        if (head == target) return idx;
        if (seen.count(head)) return -1; // 已进入循环且未找到 target
        seen.insert(head);
        head = head->next;
        ++idx;
    }
    return -1;
}

ListNode* buildList(const vector<int>& vals, int pos) {
    if (vals.empty()) return nullptr;
    vector<ListNode*> nodes;
    nodes.reserve(vals.size());
    for (int v : vals) nodes.push_back(new ListNode(v));
    for (size_t i = 0; i + 1 < nodes.size(); ++i) nodes[i]->next = nodes[i+1];
    if (pos >= 0 && pos < (int)nodes.size()) nodes.back()->next = nodes[pos];
    return nodes.front();
}

int main() {
    {
        vector<int> vals = {3,2,0,-4};
        int pos = 1;
        ListNode* head = buildList(vals, pos);
        ListNode* start = detectCycle(head);
        cout << "示例1 环起点索引: " << indexOf(head, start) << "\n"; // 期望 1
    }
    {
        vector<int> vals = {1,2};
        int pos = 0;
        ListNode* head = buildList(vals, pos);
        ListNode* start = detectCycle(head);
        cout << "示例2 环起点索引: " << indexOf(head, start) << "\n"; // 期望 0
    }
    {
        vector<int> vals = {1};
        int pos = -1;
        ListNode* head = buildList(vals, pos);
        ListNode* start = detectCycle(head);
        cout <<
    // 示例3: [1], pos = -1 -> 无环，返回 -1
    {
        vector<int> vals = {1};
        int pos = -1;
        ListNode* head = buildList(vals, pos);
        ListNode* start = detectCycle(head);
        cout << "示例3 环起点索引: " << indexOf(head, start) << "\n"; // 期望 -1
    }
    return 0;
}