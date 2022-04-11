//
// Kingsley Chen <kingsamchen at gmail dot com>
//

#pragma once

#ifndef BASE_SUBPROCESS_H_
#define BASE_SUBPROCESS_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include <unistd.h>

#include "esl/unique_handle.h"

namespace base {

class subprocess {
private:
    struct use_null_t {
        struct tag {};

        enum class mode {
            in,
            out
        };

        mode mode;
    };

    struct use_pipe_t {
        struct tag {};

        enum class mode {
            in,
            out
        };

        mode mode;
        int pfd{};
    };

public:
    static constexpr use_null_t::tag use_null;
    static constexpr use_pipe_t::tag use_pipe;

    class options {
        friend class subprocess;

    public:
        // See https://bit.ly/3rxamLo.
        options() {} // NOLINT(modernize-use-equals-default)

        options& clone_with_flags(std::uint64_t flags) noexcept {
            clone_flags_ = flags;
            return *this;
        }

        options& set_stdin(use_null_t::tag) {
            action_table_[STDIN_FILENO] = use_null_t{use_null_t::mode::in};
            return *this;
        }

        options& set_stdin(use_pipe_t::tag) {
            action_table_[STDIN_FILENO] = use_pipe_t{use_pipe_t::mode::in};
            return *this;
        }

        options& set_stdout(use_null_t::tag) {
            action_table_[STDOUT_FILENO] = use_null_t{use_null_t::mode::out};
            return *this;
        }

        options& set_stdout(use_pipe_t::tag) {
            action_table_[STDOUT_FILENO] = use_pipe_t{use_pipe_t::mode::out};
            return *this;
        }

        options& set_stderr(use_null_t::tag) {
            action_table_[STDERR_FILENO] = use_null_t{use_null_t::mode::out};
            return *this;
        }

        options& set_stderr(use_pipe_t::tag) {
            action_table_[STDERR_FILENO] = use_pipe_t{use_pipe_t::mode::out};
            return *this;
        }

    private:
        using stdio_action = std::variant<use_null_t, use_pipe_t>;
        std::uint64_t clone_flags_{};
        // TODO(KC): can replace with flatmap or ordered vector.
        std::map<int, stdio_action> action_table_;
    };

    // Spawn a process to run given commandline args.
    // `args[0]` must be fullpath to the executable.
    // Throws:
    //  - `std::invalid_argument` if `argv` is empty.
    //  - `std::system_error` for system related failures.
    explicit subprocess(const std::vector<std::string>& argv, const options& opts = options());

    ~subprocess() = default;

    // Not copyable but movable, like std::thread.

    subprocess(const subprocess&) = delete;

    subprocess& operator=(const subprocess&) = delete;

    subprocess(subprocess&&) noexcept = default;

    subprocess& operator=(subprocess&&) noexcept = default;

    void wait();

    // Returns -1 i.e. invalid fd if no corresponding pipe was set.

    int stdin_pipe() const {
        return stdio_pipes_[STDIN_FILENO].get();
    }

    int stdout_pipe() const {
        return stdio_pipes_[STDOUT_FILENO].get();
    }

    int stderr_pipe() const {
        return stdio_pipes_[STDERR_FILENO].get();
    }

private:
    // Throws `std::system_error` for system related failures.
    void spawn(std::unique_ptr<const char*[]> argvp, options& opts);

    // Throws `std::system_error` for system related failures.
    void spawn_impl(std::unique_ptr<const char*[]> argvp, const options& opts, int err_fd);

    void read_child_error_pipe(int err_fd, const char* executable);

    // Throws `std::system_error` for system related failures.
    void handle_stdio_action(int stdio_fd, const use_null_t& action);

    // Throws `std::system_error` for system related failures.
    void handle_stdio_action(int stdio_fd, const use_pipe_t& action);

private:
    pid_t pid_{};
    esl::unique_fd stdio_pipes_[3]{};
};

} // namespace base

#endif // BASE_SUBPROCESS_H_
