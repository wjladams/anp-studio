#include "cppanp/synthesis.hpp"

#include <cctype>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace cppanp {
namespace {

void normalize_map(std::map<std::string, double>& vals) {
  double total = 0.0;
  for (const auto& [_, v] : vals) {
    (void)_;
    if (v > 0.0) total += v;
  }
  if (total == 0.0) return;
  for (auto& [_, v] : vals) {
    (void)_;
    if (v < 0.0) v = 0.0;
    v /= total;
  }
}

// ---- Tiny recursive-descent expression evaluator -------------------------

class ExprParser {
public:
  ExprParser(std::string expr, const std::map<std::string, double>& vars)
      : expr_(std::move(expr)), vars_(vars) {}

  double parse() {
    skip();
    const double v = parse_expr();
    skip();
    if (pos_ != expr_.size()) {
      throw SynthesisError("unexpected trailing input in expression");
    }
    return v;
  }

private:
  std::string expr_;
  const std::map<std::string, double>& vars_;
  std::size_t pos_ = 0;

  void skip() {
    while (pos_ < expr_.size() &&
           std::isspace(static_cast<unsigned char>(expr_[pos_]))) {
      ++pos_;
    }
  }

  bool match(char c) {
    skip();
    if (pos_ < expr_.size() && expr_[pos_] == c) {
      ++pos_;
      return true;
    }
    return false;
  }

  double parse_expr() {
    double v = parse_term();
    for (;;) {
      if (match('+')) {
        v += parse_term();
      } else if (match('-')) {
        v -= parse_term();
      } else {
        break;
      }
    }
    return v;
  }

  double parse_term() {
    double v = parse_factor();
    for (;;) {
      if (match('*')) {
        v *= parse_factor();
      } else if (match('/')) {
        const double d = parse_factor();
        if (d == 0.0) {
          throw SynthesisError("division by zero in synthesis expression");
        }
        v /= d;
      } else {
        break;
      }
    }
    return v;
  }

  double parse_factor() {
    skip();
    if (match('+')) return parse_factor();
    if (match('-')) return -parse_factor();
    if (match('(')) {
      const double v = parse_expr();
      if (!match(')')) {
        throw SynthesisError("missing ')' in synthesis expression");
      }
      return v;
    }
    if (pos_ < expr_.size() &&
        (std::isdigit(static_cast<unsigned char>(expr_[pos_])) ||
         expr_[pos_] == '.')) {
      return parse_number();
    }
    return parse_ident();
  }

  double parse_number() {
    const std::size_t start = pos_;
    while (pos_ < expr_.size() &&
           (std::isdigit(static_cast<unsigned char>(expr_[pos_])) ||
            expr_[pos_] == '.')) {
      ++pos_;
    }
    try {
      return std::stod(expr_.substr(start, pos_ - start));
    } catch (...) {
      throw SynthesisError("invalid number in synthesis expression");
    }
  }

  double parse_ident() {
    skip();
    if (pos_ >= expr_.size() ||
        !(std::isalpha(static_cast<unsigned char>(expr_[pos_])) ||
          expr_[pos_] == '_')) {
      throw SynthesisError("expected identifier in synthesis expression");
    }
    const std::size_t start = pos_;
    while (pos_ < expr_.size()) {
      const char c = expr_[pos_];
      if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
        ++pos_;
      } else {
        break;
      }
    }
    const std::string name = expr_.substr(start, pos_ - start);
    const auto it = vars_.find(name);
    if (it != vars_.end()) return it->second;
    return 0.0;
  }
};

}  // namespace

std::map<std::string, double> synthesize_additive(
    const std::map<std::string, double>& subnet_weights,
    const std::map<std::string, std::map<std::string, double>>& alt_scores) {
  double weight_total = 0.0;
  for (const auto& [name, _] : alt_scores) {
    (void)_;
    const auto wit = subnet_weights.find(name);
    if (wit != subnet_weights.end()) weight_total += wit->second;
  }

  std::map<std::string, double> rval;
  std::map<std::string, double> counts;
  for (const auto& [subnet_name, vals] : alt_scores) {
    double priority = 0.0;
    const auto wit = subnet_weights.find(subnet_name);
    if (wit != subnet_weights.end()) priority = wit->second;
    if (weight_total != 0.0) priority /= weight_total;
    for (const auto& [alt_name, val] : vals) {
      rval[alt_name] += val * priority;
      counts[alt_name] += priority;
    }
  }
  for (auto& [alt_name, val] : rval) {
    const double c = counts[alt_name];
    if (c > 0.0) val /= c;
  }
  return rval;
}

std::map<std::string, double> synthesize_multiplicative(
    const std::map<std::string, double>& subnet_weights,
    const std::map<std::string, std::map<std::string, double>>& alt_scores) {
  double weight_total = 0.0;
  for (const auto& [name, _] : alt_scores) {
    (void)_;
    const auto wit = subnet_weights.find(name);
    if (wit != subnet_weights.end()) weight_total += wit->second;
  }

  std::map<std::string, double> rval;
  bool first = true;
  for (const auto& [subnet_name, vals] : alt_scores) {
    double w = 0.0;
    const auto wit = subnet_weights.find(subnet_name);
    if (wit != subnet_weights.end()) w = wit->second;
    if (weight_total != 0.0) w /= weight_total;

    if (first) {
      for (const auto& [alt, val] : vals) {
        rval[alt] = (val <= 0.0) ? 0.0 : std::pow(val, w);
      }
      first = false;
    } else {
      for (auto& [alt, acc] : rval) {
        const auto vit = vals.find(alt);
        const double val = vit == vals.end() ? 0.0 : vit->second;
        if (acc <= 0.0 || val <= 0.0) {
          acc = 0.0;
        } else {
          acc *= std::pow(val, w);
        }
      }
      for (const auto& [alt, val] : vals) {
        if (rval.find(alt) == rval.end()) {
          rval[alt] = 0.0;
          (void)val;
        }
      }
    }
  }
  normalize_map(rval);
  return rval;
}

std::map<std::string, double> synthesize_custom(
    const std::string& expression,
    const std::map<std::string, std::map<std::string, double>>& alt_scores,
    const std::vector<std::string>& alt_order) {
  if (expression.empty()) {
    throw SynthesisError("custom synthesis expression is empty");
  }

  std::map<std::string, double> rval;
  for (const std::string& alt : alt_order) {
    std::map<std::string, double> vars;
    for (const auto& [subnet_name, scores] : alt_scores) {
      const auto it = scores.find(alt);
      vars[subnet_name] = it == scores.end() ? 0.0 : it->second;
    }
    try {
      rval[alt] = eval_expression(expression, vars);
    } catch (const SynthesisError&) {
      throw;
    }
  }
  normalize_map(rval);
  return rval;
}

std::map<std::string, double> synthesize(
    const SynthesisOptions& options,
    const std::map<std::string, double>& subnet_weights,
    const std::map<std::string, std::map<std::string, double>>& alt_scores,
    const std::vector<std::string>& alt_order) {
  switch (options.kind) {
    case SynthesisKind::Additive:
      return synthesize_additive(subnet_weights, alt_scores);
    case SynthesisKind::Multiplicative:
      return synthesize_multiplicative(subnet_weights, alt_scores);
    case SynthesisKind::Custom:
      return synthesize_custom(options.custom_expr, alt_scores, alt_order);
  }
  return synthesize_additive(subnet_weights, alt_scores);
}

double eval_expression(const std::string& expression,
                       const std::map<std::string, double>& variables) {
  ExprParser parser(expression, variables);
  return parser.parse();
}

}  // namespace cppanp
