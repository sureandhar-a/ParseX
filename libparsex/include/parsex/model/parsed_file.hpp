#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <parsex/model/cluster.hpp>
#include <parsex/model/ecu_instance.hpp>
#include <parsex/model/frame.hpp>
#include <parsex/model/pdu.hpp>
#include <parsex/model/signal.hpp>
#include <parsex/model/signal_group.hpp>
#include <parsex/raw/raw_document.hpp>
#include <parsex/raw/raw_span.hpp>

struct Warning {
    std::string message;
    std::optional<RawSpan> location;
};

struct ParsedFile {
    std::string autosarRelease;
    std::filesystem::path sourcePath;
    std::vector<Cluster> clusters;
    std::vector<EcuInstance> ecuInstances;
    std::vector<Frame> frames;
    std::vector<Pdu> pdus;
    std::vector<Signal> signals;
    std::vector<SignalGroup> signalGroups;
    std::vector<Warning> warnings;
    std::shared_ptr<RawDocument> rawDocument;
};
