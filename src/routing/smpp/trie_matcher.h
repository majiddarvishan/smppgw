#pragma once

#include "bit_set.h"

#include <iostream>

#define INTEGERS 10
#define ALPHABETS 59 //all printable characters except uppercase letters and digits

using ruleId_t = uint32_t;
using catId_t = uint32_t;

/**
 * @brief The trie_matcher class, this trie works just with digits and lowercase letters
 */
class trie_matcher
{
public:
    enum class entry_type
    {
        prefix,
        complete
    };

    class node
    {
    public:
        node(size_t rulesCount)
            : mark_Complete(rulesCount)
            , mark_prefix(rulesCount)
        {
        }

        ~node() = default;
        node* children[INTEGERS] = { nullptr };
        node** childrenAlphabet = nullptr;
        bit_set mark_Complete;
        bit_set mark_prefix;
    };

    trie_matcher(size_t t_CategoryCounts);
    ~trie_matcher();

    void add_entry(const std::string& t_prefixe, catId_t t_ruleTag, entry_type t_entryType);
    void add_entry(const std::string& t_prefixe, catId_t t_ruleTag);

    bit_set match(const std::string& t_query) const;
    size_t get_trie_size() const
    {
        return byte_counts_;
    }

private:
    void remove_trie(node* node);
    void insert_node(node* node, const std::string& word, size_t index, ruleId_t t_ruleTag, entry_type t_entryType);
    void trace_nodes(node* node, const std::string& word, size_t index, bit_set& result) const;
    node* get_root();

private:
    size_t category_count_ = 0;
    size_t byte_counts_ = 0;
    node* root_;
    size_t node_size_ = 0;
    std::array<int, 256> alphabets_map_;
};
