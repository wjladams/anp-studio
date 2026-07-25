#pragma once

#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace cppanp {

enum class SynthesisKind {
  Additive,        // Weighted average of subnet scores (V1 / pyanp default).
  Multiplicative,  // Product of subnet scores across control nodes, then normalize.
  Custom,          // Expression over subnet-host names (+ - * / and parentheses).
};

struct SynthesisOptions {
  SynthesisKind kind = SynthesisKind::Additive;
  // Used when kind == Custom. Identifiers are subnet-host node names.
  // Example: "Benefits / Costs" or "Benefits * Opportunities / (Costs * Risks)".
  std::string custom_expr;
};

class SynthesisError : public std::runtime_error {
public:
  explicit SynthesisError(const std::string& message)
      : std::runtime_error(message) {}
};

// Additive (weighted average), matching AnpNetwork::sum_subnetwork_formula.
[[nodiscard]] std::map<std::string, double> synthesize_additive(
    const std::map<std::string, double>& subnet_weights,
    const std::map<std::string, std::map<std::string, double>>& alt_scores);

// Multiplicative: for each alt, product over subnets of score^weight (with
// renormalized weights), then L1-normalize across alts. Zero scores stay zero.
[[nodiscard]] std::map<std::string, double> synthesize_multiplicative(
    const std::map<std::string, double>& subnet_weights,
    const std::map<std::string, std::map<std::string, double>>& alt_scores);

// Custom expression: for each alternative, substitute each subnet-host name
// with that subnet's score for the alt, evaluate the expression, then
// L1-normalize positive totals (negative results clamped to 0 before norm).
[[nodiscard]] std::map<std::string, double> synthesize_custom(
    const std::string& expression,
    const std::map<std::string, std::map<std::string, double>>& alt_scores,
    const std::vector<std::string>& alt_order);

// Dispatch on SynthesisOptions.
[[nodiscard]] std::map<std::string, double> synthesize(
    const SynthesisOptions& options,
    const std::map<std::string, double>& subnet_weights,
    const std::map<std::string, std::map<std::string, double>>& alt_scores,
    const std::vector<std::string>& alt_order);

// Evaluate a numeric expression with named variables (for unit tests / GUI).
[[nodiscard]] double eval_expression(
    const std::string& expression,
    const std::map<std::string, double>& variables);

}  // namespace cppanp
