#include "prefix_rule_matcher.h"

#include <algorithm>
#include <cassert>

prefix_rule_matcher::prefix_rule_matcher(size_t t_rulesCount, size_t t_ruleParameterCount)
    : rule_parameter_count_(t_ruleParameterCount)
    , rules_count_(t_rulesCount)
    , trie_(t_rulesCount * t_ruleParameterCount)
    , match_mask_(t_rulesCount * t_ruleParameterCount)
{
    for(size_t i = 0; i < t_rulesCount; i++)
    {
        match_mask_.set((i + 1) * t_ruleParameterCount - 1);
    }
}

ruleId_t prefix_rule_matcher::set_rule(const std::vector<std::vector<std::string> >& t_rule)
{
    assert(t_rule.size() == rule_parameter_count_);
    ruleId_t ruleId = count_of_set_rules_++;

    for(size_t i = 0; i < rule_parameter_count_; i++)
    {
        for(size_t j = 0; j < t_rule[i].size(); j++)
        {
            for(auto& ch : t_rule[i][j])
            {
                if(isupper(ch))
                {
                    throw std::runtime_error("Uppercase letter is not allowed");
                }
            }

            trie_.add_entry(t_rule[i][j], (ruleId * rule_parameter_count_ + i));
        }
    }

    return ruleId;
} //prefix_rule_matcher::set_rule

std::vector<ruleId_t> prefix_rule_matcher::match(const std::vector<std::string>& t_record) const
{
    assert(t_record.size() == rule_parameter_count_);
    std::vector<ruleId_t> ret;
    ret.reserve(rules_count_);

    auto result = match_get_bitset(t_record);
    auto matchedRules = result.get_set_bits_indices();

    for(auto item : matchedRules)
    {
        ret.push_back(((item + 1) / rule_parameter_count_) - 1);
    }

    return ret;
}

bit_set prefix_rule_matcher::match_get_bitset(const std::vector<std::string>& t_record) const
{
    assert(t_record.size() == rule_parameter_count_);
    auto result = trie_.match(t_record[0]);

    for(size_t i = 1; i < rule_parameter_count_; i++)
    {
        result.shift_right();
        result &= trie_.match(t_record[i]);
    }

    result &= match_mask_;
    return result;
}
