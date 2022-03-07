#pragma once

struct test_object {
  int a = 0;
  test_object() = default;
  explicit test_object(int b) : a(b) {}
  test_object(test_object &&other) noexcept { std::swap(a, other.a); }
  friend bool operator<(test_object const &c, test_object const &b) {
    return c.a < b.a;
  }
  friend bool operator==(test_object const &c, test_object const &b) {
    return c.a == b.a;
  }
};

struct vector_compare {
  using vec = std::pair<int, int>;
  enum distance_type { euclidean, manhattan };

  explicit vector_compare(distance_type p = euclidean) : type(p) {}

  bool operator()(vec a, vec b) const {
    if (type == euclidean) {
      return euc(a) < euc(b);
    } else {
      return man(a) < man(b);
    }
  }

private:
  static double euc(vec x) {
    return sqrt(x.first * x.first + x.second * x.second);
  }

  static double man(vec x) { return abs(x.first) + abs(x.second); }

  distance_type type;
};

struct non_default_constructible {
  non_default_constructible() = delete;
  explicit non_default_constructible(int b) : a(b) {}
  non_default_constructible(non_default_constructible const &) = default;
  friend bool operator<(non_default_constructible const &c, non_default_constructible const &b) {
    return c.a < b.a;
  }
  friend bool operator==(non_default_constructible const &c, non_default_constructible const &b) {
    return c.a == b.a;
  }
private:
  int a;
};

