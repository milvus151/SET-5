#include <functional>
#include <iostream>
#include <map>
#include <random>
#include <unordered_set>

class RandomStreamGen {
  size_t stream_size;
  size_t ready_strings_count;
  std::mt19937 rsg;
  std::string generateOneString() {
    static const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-";
    size_t alphabet_size = 62;
    std::uniform_int_distribution dist_len(1, 30);
    std::uniform_int_distribution<> dist_char(0, alphabet_size);
    size_t length = dist_len(rsg);
    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
      result += charset[dist_char(rsg)];
    }
    return result;
  }

public:
  explicit RandomStreamGen(size_t size) {
    stream_size = size;
    ready_strings_count = 0;
    std::random_device rd;
    rsg.seed(rd());
  }
  std::vector<std::string> nextPortion(double percent) {
    std::vector<std::string> portion;
    auto count_to_generate = static_cast<size_t>(stream_size * percent);

    if (ready_strings_count + count_to_generate > stream_size) {
      count_to_generate = stream_size - ready_strings_count;
    }
    if (count_to_generate == 0) {
      return portion;
    }
    for (size_t i = 0; i < count_to_generate; ++i) {
      portion.push_back(generateOneString());
    }
    ready_strings_count += count_to_generate;
    return portion;
  }
  bool isFinished() const { return ready_strings_count >= stream_size; }
};

class HashFuncGen {
  std::mt19937 rhg;

public:
  HashFuncGen() {
    std::random_device rd;
    rhg.seed(rd());
  }
  auto generateNewHashFunc() {
    std::uniform_int_distribution<unsigned int> dist(1000, 4294967295);
    unsigned int A = dist(rhg);
    A = A | 1;
    return [A](const std::string &s) -> unsigned int {
      unsigned int hash = 0;
      for (char c: s) {
        hash = hash * A + static_cast<unsigned int>(c);
      }
      return hash;
    };
  }
};

class HyperLogLog {
  size_t B;
  size_t m;
  std::vector<int> subStreams;
  std::function<unsigned int(const std::string &)> h;

public:
  HyperLogLog(size_t b, HashFuncGen &hasher) {
    B = b;
    m = 1 << B;
    subStreams = std::vector<int>(m, 0);
    h = hasher.generateNewHashFunc();
  }
  void workWithStrings(std::vector<std::string> &myStrings) {
    for (std::string str: myStrings) {
      unsigned int hash = h(str);
      size_t index = hash >> (32 - B);
      unsigned int rank = std::countl_zero(hash << B) + 1;
      if (rank > subStreams[index]) {
        subStreams[index] = rank;
      }
    }
  }
  double get_alpha(size_t m_val) {
    if (m_val == 2)
      return 0.3512;
    if (m_val == 4)
      return 0.5324;
    if (m_val == 8)
      return 0.6355;
    if (m_val == 16)
      return 0.673;
    if (m_val == 32)
      return 0.697;
    if (m_val == 64)
      return 0.709;
    if (m_val >= 128)
      return 0.7213 / (1.0 + 1.079 / m_val);
    return 0.673;
  }
  double approx() {
    double sum = 0;
    for (int i = 0; i < m; i++) {
      sum += std::pow(2.0, -subStreams[i]);
    }
    double res = get_alpha(m) * m * m / sum;
    if (res < 2.5 * m) {
      int v = 0;
      for (int t: subStreams) {
        if (t == 0) {
          v++;
        }
      }
      if (v == 0) {
        return res;
      }
      res = m * std::log(static_cast<double>(m) / v);
    }
    return res;
  }
};

std::pair<double, double> statsCounter(std::vector<double> &results) {
  size_t n = results.size();
  double sum = 0.0;
  for (double t: results) {
    sum += t;
  }
  double E = sum / static_cast<double>(n);
  double sumDelux = 0.0;
  for (double t: results) {
    sumDelux += (t - E) * (t - E);
  }
  double sigma = std::sqrt(sumDelux / (n - 1));
  return std::make_pair(E, sigma);
}
class ExactCounter {
  std::unordered_set<std::string> unique_elements;

public:
  void add(const std::vector<std::string> &portion) {
    for (const auto &str: portion) {
      unique_elements.insert(str);
    }
  }
  size_t get() const { return unique_elements.size(); }
  void clear() { unique_elements.clear(); }
};

// main для 1 эксперимента
int main() {
  for (int b = 6; b <= 14; b += 4) {
    std::cout << "B = " << b << "\n";
    RandomStreamGen stream(1000000);
    HashFuncGen hasher;
    HyperLogLog hyper_log_log(b, hasher);
    ExactCounter real_counter;
    double percent_counter = 0.0;
    double my_working_percent = 0.05;
    while (!stream.isFinished()) {
      auto data = stream.nextPortion(my_working_percent);
      percent_counter += my_working_percent;
      hyper_log_log.workWithStrings(data);
      real_counter.add(data);
      double approx_res = hyper_log_log.approx();
      size_t realResult = real_counter.get();
      std::cout << percent_counter * 100 << "%" << " " << realResult << " " << approx_res << "\n";
    }
    std::cout << "__________________________\n";
  }
  return 0;
}

// main для 2 эксперимента
/*int main() {
  for (int b = 6; b <= 14; b += 4) {
    std::cout << "B = " << b << "\n";
    std::vector<std::vector<double>> statistics_data(101);
    for (int i = 0; i < 100; i++) {
      RandomStreamGen stream(100000);
      HashFuncGen hasher;
      HyperLogLog hyper_log_log(b, hasher);
      std::unordered_set<std::string> global_set;
      double percent_counter = 0.0;
      double my_working_percent = 0.05;
      while (!stream.isFinished()) {
        auto data = stream.nextPortion(my_working_percent);
        percent_counter += my_working_percent;
        hyper_log_log.workWithStrings(data);
        double approx_res = hyper_log_log.approx();
        int index = std::round(percent_counter * 100);
        statistics_data[index].push_back(approx_res);
      }
    }
    std::vector<double> Es;
    std::vector<double> sigmas;
    for (int i = 5; i <= 100; i += 5) {
      std::pair<double, double> stats = statsCounter(statistics_data[i]);
      Es.push_back(stats.first);
      sigmas.push_back(stats.second);
    }
    for (int i = 0; i < 20; i++) {
      std::cout <<5*(i+1)<<"% "<< "E = " << Es[i] << "; sigma = " << sigmas[i]<<"\n";
    }
    std::cout << "__________________________\n";
  }
  return 0;
}
*/