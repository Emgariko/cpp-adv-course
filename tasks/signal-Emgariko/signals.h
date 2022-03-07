#pragma once
#include "intrusive_list.h"
#include <functional>

// Чтобы не было коллизий с UNIX-сигналами реализация вынесена в неймспейс, по
// той же причине изменено и название файла
namespace signals {

template <typename T>
struct signal;

template<typename... Args>
struct signal<void(Args...)> {
  using slot_t = std::function<void(Args...)>;

  struct connection_tag;
  struct connection : intrusive::list_element<connection_tag> {

    connection() : sig(nullptr), slot() {}

    connection(const connection& other) = delete;

    connection(connection&& other) : sig(other.sig), slot(other.slot) {
      if (other.sig) {
        typename intrusive::list<connection, connection_tag>::iterator it(
            &other);
        sig->cons.insert(++it, *this);
        other.disconnect();
      }
    }

    connection& operator=(const connection& other) {
      sig = other.sig;
      slot = other.slot;
      return *this;
    }

    connection& operator=(connection&& other) {
      if (this == &other) {
        return *this;
      }

      slot = std::move(other.slot);
      sig = other.sig;
      if (other.sig) {
        typename intrusive::list<connection, connection_tag>::iterator it(
            &other);
        sig->cons.insert(++it, *this);
        other.disconnect();
      } else if (sig) {
        disconnect();
      }
      return *this;
    }

    void disconnect() {
      if (sig) {
        for (iteration_token* tok = sig->top_token; tok != nullptr; tok = tok->next) {
          if (&*tok->it == this) {
            ++(tok->it);
          }
        }
        this->unlink();
        sig = nullptr;
        slot = {};
      }
    }

    ~connection() {
      disconnect();
    }

    friend signal;
  private:
    signal* sig;
    slot_t slot;

    connection(signal* sig, slot_t slot) : sig(sig), slot(slot) {
      sig->cons.push_back(*this);
    }
  };

  using connections_t = intrusive::list<connection, connection_tag>;

  signal() = default;

  ~signal() {
    for (auto it = cons.begin(); it != cons.end(); it++) {
      if (it->sig) {
        it->sig = nullptr;
        it->slot = {};
      }
    }
    cons.clear();
    for (iteration_token* tok = top_token; tok != nullptr; tok = tok->next) {
      tok->sig = nullptr;
    }
  }

  connection connect(slot_t slot) {
    return connection(this, slot);
  }

  struct iteration_token {
    explicit iteration_token(const signal* sig) : sig(sig), it(sig->cons.begin()), next(sig->top_token) {
      sig->top_token = this;
    }

    ~iteration_token() {
      if (sig) {
        sig->top_token = next;
      }
    }

    const signal* sig; // nullptr = sig not exists
    typename connections_t::const_iterator it;
    iteration_token* next;
  };

  template<typename... Args1>
  void operator()(Args1... args) const {
    iteration_token tok(this);
    while (tok.it != cons.end()) {
      typename connections_t::const_iterator cur = tok.it;
      ++tok.it;
      cur->slot(args...);
      if (tok.sig == nullptr) {
        break;
      }
    }
  }

private:
  connections_t cons;
  mutable iteration_token* top_token = nullptr;
};

} // namespace signals
