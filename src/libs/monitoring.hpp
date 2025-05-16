#pragma once

#include <prometheus/counter.h>
#include <prometheus/gauge.h>
#include <prometheus/family.h>

#include <pa/config.hpp>

#include <map>
#include <string>

inline prometheus::Counter& add_counter(prometheus::Family<prometheus::Counter>& family, const std::shared_ptr<pa::config::node>& config, std::map<std::string, std::string> lables)
{
    for(const auto& label : config->nodes())
    {
        lables.emplace(label->at("key")->get<std::string>(), label->at("value")->get<std::string>());
    }

    return family.Add(lables);
}

inline prometheus::Gauge& add_gauge(prometheus::Family<prometheus::Gauge>& family, const std::shared_ptr<pa::config::node>& config, std::map<std::string, std::string> lables)
{
    for(const auto& label : config->nodes())
    {
        lables.emplace(label->at("key")->get<std::string>(), label->at("value")->get<std::string>());
    }

    return family.Add(lables);
}

inline void remove_counter(prometheus::Family<prometheus::Counter>& family, prometheus::Counter *metric )
{
    return family.Remove(metric);
}

inline void remove_gauge(prometheus::Family<prometheus::Gauge>& family, prometheus::Gauge *metric)
{
    return family.Remove(metric);
}