#include "trie_matcher.h"

trie_matcher::trie_matcher(size_t t_CategoryCounts)
    : category_count_(t_CategoryCounts)
    , root_(new node(t_CategoryCounts))
{
    alphabets_map_.fill(-1);
    int ind = 0;

    for(size_t i = 0; i < 256; i++)  //use all printable characters
    {
        if(isprint(static_cast<int>(i)) && !isupper(static_cast<int>(i)) && !isdigit(static_cast<int>(i)))
        {
            alphabets_map_[i] = ind++;
        }
    }
}

trie_matcher::~trie_matcher()
{
    remove_trie(root_);
}

void trie_matcher::add_entry(const std::string& t_prefix, catId_t t_ruleTag, entry_type t_entryType)
{
    if(t_prefix.length() == 0)  //empty word
    {
        if(t_entryType == entry_type::prefix)
        {
            root_->mark_prefix.set(t_ruleTag);
        }
        else if(t_entryType == entry_type::complete)
        {
            root_->mark_Complete.set(t_ruleTag);
        }
    }
    else
    {
        insert_node(root_, t_prefix, 0, t_ruleTag, t_entryType);
    }
} //trie_matcher::add_entry

void trie_matcher::add_entry(const std::string& t_prefixe, catId_t t_ruleTag)
{
    if(t_prefixe[t_prefixe.length() - 1] == '*')
    {
        add_entry(t_prefixe.substr(0, t_prefixe.length() - 1), t_ruleTag, entry_type::prefix);
    }
    else
    {
        add_entry(t_prefixe, t_ruleTag, entry_type::complete);
    }
}

bit_set trie_matcher::match(const std::string& t_query) const
{
    std::string query(t_query);

    for(char& ch : query)
    {
        if(isupper(ch))
        {
            ch = tolower(ch);
        }
    }

    bit_set ret(category_count_);

    if(query == "*")
    {
        ret |= root_->mark_prefix;
    }
    else if(query.length() == 0)
    {
        //empty parameter just match with complete empty (not prefix empty)
        ret |= root_->mark_Complete;
    }
    else
    {
        trace_nodes(root_, query, 0, ret);
    }

    return ret;
} //trie_matcher::Match

void trie_matcher::remove_trie(node* n)
{
    for(int i = 0; i < INTEGERS; i++)
    {
        if(n->children[i] != NULL)
        {
            remove_trie(n->children[i]);
        }
    }

    if(n->childrenAlphabet != nullptr)
    {
        for(int i = 0; i < ALPHABETS; i++)
        {
            if(n->childrenAlphabet[i] != NULL)
            {
                remove_trie(n->childrenAlphabet[i]);
            }
        }

        delete[] n->childrenAlphabet;
    }

    delete n;
} //trie_matcher::RemoveTrie

void trie_matcher::insert_node(trie_matcher::node* n, const std::string& word, size_t index, ruleId_t t_ruleTag, entry_type t_entryType)
{
    auto alphabetIndex = alphabets_map_.at(static_cast<size_t>(word[index]));

    if(alphabetIndex > -1)
    {
        if(n->childrenAlphabet == nullptr)
        {
            n->childrenAlphabet = new trie_matcher::node*[ALPHABETS] { nullptr };
            byte_counts_ += sizeof(n) * ALPHABETS;
        }

        if(!n->childrenAlphabet[alphabetIndex])
        {
            n->childrenAlphabet[alphabetIndex] = new trie_matcher::node(category_count_);
            byte_counts_ += sizeof(n);
        }

        if(index == word.length() - 1)
        {
            if(t_entryType == entry_type::prefix)
            {
                n->childrenAlphabet[alphabetIndex]->mark_prefix.set(t_ruleTag);
            }
            else if(t_entryType == entry_type::complete)
            {
                n->childrenAlphabet[alphabetIndex]->mark_Complete.set(t_ruleTag);
            }

            return;
        }

        return insert_node(n->childrenAlphabet[alphabetIndex], word, index + 1, t_ruleTag, t_entryType);
    }
    else if(isdigit(word[index]))
    {
        if(!n->children[word[index] - '0'])
        {
            n->children[word[index] - '0'] = new trie_matcher::node(category_count_);
            byte_counts_ += sizeof(n);
        }

        if(index == word.length() - 1)
        {
            if(t_entryType == entry_type::prefix)
            {
                n->children[word[index] - '0']->mark_prefix.set(t_ruleTag);
            }
            else if(t_entryType == entry_type::complete)
            {
                n->children[word[index] - '0']->mark_Complete.set(t_ruleTag);
            }

            return;
        }

        return insert_node(n->children[word[index] - '0'], word, index + 1, t_ruleTag, t_entryType);
    }
    else
    {
        throw std::runtime_error("Invalid character in query word: " + word);
    }
} //trie_matcher::insert_node

void trie_matcher::trace_nodes(node* n, const std::string& word, size_t index, bit_set& result) const
{
    if(!n)
    {
        return;
    }

    result |= n->mark_prefix;

    if(index == word.length())
    {
        result |= n->mark_Complete;
        return;
    }

    auto alphabetIndex = alphabets_map_.at(static_cast<size_t>(word[index]));

    if(alphabetIndex > -1)
    {
        if(n->childrenAlphabet == nullptr)
        {
            return;
        }

        return trace_nodes(n->childrenAlphabet[alphabetIndex], word, index + 1, result);
    }
    else if(isdigit(word[index]))
    {
        return trace_nodes(n->children[word[index] - '0'], word, index + 1, result);
    }
    else
    {
        throw std::runtime_error("Invalid character in query word: " + word);
    }
} //trie_matcher::trace_nodes
