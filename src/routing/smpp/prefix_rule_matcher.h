#pragma once

#include "trie_matcher.h"

class prefix_rule_matcher
{
public:
    prefix_rule_matcher(size_t t_rulesCount, size_t t_ruleParameterCount);
    virtual ~prefix_rule_matcher() = default;
    /**
     * @brief SetRule, case of input strings should be lowercase
     * @param t_rule
     * @return
     */
    ruleId_t set_rule(const std::vector<std::vector<std::string> >& t_rule);
    std::vector<ruleId_t> match(const std::vector<std::string>& t_record) const;
    /**
     * @brief match_get_bitset, returns a bit_set that bit of [ruleNumber * t_ruleParameterCount]-1
     * for matched rule is set. case of input strings should be lowercase
     * @param t_record
     * @return
     */
    bit_set match_get_bitset(const std::vector<std::string>& t_record) const;

private:
    size_t rule_parameter_count_;
    size_t rules_count_;
    size_t count_of_set_rules_ = 0;
    trie_matcher trie_;
    bit_set match_mask_;
};
