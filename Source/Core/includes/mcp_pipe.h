#pragma once

// ponytail: mcp_server.inl keeps its own copy until it migrates to this header
//
// The Luma MCP pipe transport (DEVELOPMENT only), for a backend without core.hpp's prelude (the native D3D9 module, d3d9/mcp.hpp).
// A named pipe ("\\.\pipe\luma-mcp-<pid>") serves any number of clients, usually "Scripts/luma_mcp.py" (the stdio MCP bridge). Their
// requests share one queue, run one at a time by "Drain()" on the game's render thread at present.
// Wire format, both ways: little endian u32 byte length + UTF-8 payload.
// Request: the tool name line, then "key=value" lines. Response: one JSON object with an "ok" field.
// Depends on Windows and the standard library only.

#include <Windows.h>
#include <sddl.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#pragma comment(lib, "advapi32.lib") // The pipe's security descriptor

namespace McpPipe
{
   class JsonWriter
   {
   public:
      std::string out;

      JsonWriter& BeginObject()
      {
         Separate();
         out += '{';
         comma = false;
         return *this;
      }
      JsonWriter& EndObject()
      {
         out += '}';
         comma = true;
         return *this;
      }
      JsonWriter& BeginArray()
      {
         Separate();
         out += '[';
         comma = false;
         return *this;
      }
      JsonWriter& EndArray()
      {
         out += ']';
         comma = true;
         return *this;
      }
      JsonWriter& Key(std::string_view key)
      {
         Separate();
         WriteString(key);
         out += ':';
         comma = false;
         return *this;
      }
      JsonWriter& Value(std::string_view value)
      {
         Separate();
         WriteString(value);
         comma = true;
         return *this;
      }
      // Otherwise pointers would pick the bool overload
      JsonWriter& Value(const char* value)
      {
         return Value(std::string_view(value ? value : ""));
      }
      JsonWriter& Value(bool value)
      {
         Separate();
         out += value ? "true" : "false";
         comma = true;
         return *this;
      }
      JsonWriter& Null()
      {
         Separate();
         out += "null";
         comma = true;
         return *this;
      }
      JsonWriter& Value(double value)
      {
         Separate();
         // JSON has no NaN/Inf literals
         if (std::isfinite(value))
         {
            std::format_to(std::back_inserter(out), "{}", value);
         }
         else
         {
            std::format_to(std::back_inserter(out), "\"{}\"", value);
         }
         comma = true;
         return *this;
      }
      template <typename T>
         requires std::is_integral_v<T> && (!std::is_same_v<T, bool>)
      JsonWriter& Value(T value)
      {
         Separate();
         out += std::to_string(value);
         comma = true;
         return *this;
      }
      template <typename T>
      JsonWriter& Field(std::string_view key, const T& value)
      {
         Key(key);
         return Value(value);
      }
      // "0x1234ABCD", as the bridge and the other backends write shader hashes
      JsonWriter& Hash(std::string_view key, uint32_t hash)
      {
         return Field(key, std::format("0x{:08X}", hash));
      }
      // As UTF-8: "path::string()" uses the ANSI code page and throws on characters outside it
      JsonWriter& Path(std::string_view key, const std::filesystem::path& path)
      {
         const std::u8string utf8 = path.u8string();
         return Field(key, std::string_view(reinterpret_cast<const char*>(utf8.data()), utf8.size()));
      }

   private:
      bool comma = false;

      void Separate()
      {
         if (comma)
         {
            out += ',';
         }
      }
      void WriteString(std::string_view value)
      {
         out += '"';
         for (const char c : value)
         {
            switch (c)
            {
            case '"':
               out += "\\\"";
               break;
            case '\\':
               out += "\\\\";
               break;
            case '\n':
               out += "\\n";
               break;
            case '\r':
               out += "\\r";
               break;
            case '\t':
               out += "\\t";
               break;
            default:
               if (static_cast<unsigned char>(c) < 0x20)
               {
                  out += std::format("\\u{:04x}", static_cast<unsigned char>(c));
               }
               else
               {
                  out += c;
               }
            }
         }
         out += '"';
      }
   };

   struct Job
   {
      std::string tool;
      std::unordered_map<std::string, std::string> args;
      mutable std::unordered_set<std::string> read_args; // To report the arguments no tool read (typos, renamed keys)
      JsonWriter result;
      std::string error;
      HANDLE done = CreateEventW(nullptr, TRUE, FALSE, nullptr);
      // Readbacks, written to disk by the pipe thread before it answers, so the render thread only pays for the copy
      std::vector<std::pair<std::filesystem::path, std::vector<char>>> files;
      // Multi frame jobs (render thread only)
      uint32_t phase = 0;
      uint32_t frames = 0;

      Job()
      {
         result.BeginObject().Field("ok", true);
      }
      ~Job()
      {
         CloseHandle(done);
      }

      const std::string* Arg(const char* key) const
      {
         read_args.emplace(key);
         const auto it = args.find(key);
         return it != args.end() && !it->second.empty() ? &it->second : nullptr;
      }
      int64_t IntArg(const char* key, int64_t default_value) const
      {
         const std::string* value = Arg(key);
         return value ? std::strtoll(value->c_str(), nullptr, 0) : default_value;
      }
      int64_t IntArg(const char* key, int64_t default_value, int64_t min, int64_t max) const
      {
         return std::clamp(IntArg(key, default_value), min, max);
      }
      bool BoolArg(const char* key, bool default_value) const
      {
         const std::string* value = Arg(key);
         return value ? (*value == "1" || *value == "true") : default_value;
      }
      // Accepts "0x1234ABCD" or "1234ABCD"
      std::optional<uint32_t> HashArg(const char* key) const
      {
         const std::string* value = Arg(key);
         if (!value)
         {
            return std::nullopt;
         }
         return uint32_t(std::strtoul(value->c_str(), nullptr, 16));
      }
   };

   inline std::mutex s_mutex_jobs;
   inline std::vector<std::shared_ptr<Job>> pending_jobs; // Guarded by "s_mutex_jobs"
   inline std::string active_tool;                        // Guarded by "s_mutex_jobs", the running job's, for the status and timeout replies
   inline std::atomic<bool> has_pending_jobs = false;     // Set under "s_mutex_jobs", read without it
   inline std::shared_ptr<Job> active_job;                // Render thread only ("Drain()")
   inline std::thread server_thread;
   inline std::atomic<bool> server_stop = false;
   inline std::atomic<bool> server_running = false;
   inline std::atomic<int> connected_clients = 0; // Lets a backend skip per draw bookkeeping nobody would read
   // Written at every present, so "status" can answer from the pipe thread while the game doesn't present
   inline std::atomic<uint64_t> last_present_ms = 0; // GetTickCount64()
   inline std::atomic<uint32_t> last_frame_index = 0;
   inline std::wstring pipe_name;
   // "Start()"'s, for the reply without presents
   inline std::string game_name;
   inline std::filesystem::path exe_path;

   // Where readbacks go ("out_dir" can only pick a subfolder): an elevated game must not write where a non elevated client asks.
   // Not under the game's "Luma" folder, a non empty one there would shadow the repository shaders.
   inline std::filesystem::path ReadbackRoot()
   {
      wchar_t temp_path[MAX_PATH] = {};
      GetTempPathW(MAX_PATH, temp_path);
      return std::filesystem::path(temp_path) / "luma-mcp";
   }

   // Request arguments are UTF-8 (a narrow "path" would read them in the ANSI code page)
   inline std::filesystem::path Utf8Path(std::string_view utf8)
   {
      return std::u8string_view(reinterpret_cast<const char8_t*>(utf8.data()), utf8.size());
   }

   // "dir" is "ReadbackRoot()" or inside it, ".." and junctions resolved. Never throws (pipe thread).
   // ponytail: checked, then written (a junction swapped in between wins), fine for a DEVELOPMENT only tool
   inline bool IsInReadbackRoot(const std::filesystem::path& dir)
   {
      std::error_code ec;
      const std::filesystem::path root = std::filesystem::weakly_canonical(ReadbackRoot(), ec);
      if (ec || root.empty())
      {
         return false;
      }
      const std::filesystem::path canonical_dir = std::filesystem::weakly_canonical(dir, ec);
      return !ec && std::mismatch(root.begin(), root.end(), canonical_dir.begin(), canonical_dir.end()).first == root.end();
   }

   // "<out_dir>/<stem>_p<pid>_f<frame>.bin": every Luma process shares the root, and two of them with the same shaders reach the same frame
   inline std::filesystem::path ReadbackPath(const Job& job, std::string_view stem, uint32_t frame)
   {
      const std::string file = std::format("{}_p{}_f{}.bin", stem, GetCurrentProcessId(), frame);
      if (const std::string* out_dir = job.Arg("out_dir"))
      {
         return Utf8Path(*out_dir) / file;
      }
      return ReadbackRoot() / file;
   }

   inline bool ReadExact(HANDLE pipe, void* data, DWORD size)
   {
      auto* bytes = static_cast<char*>(data);
      while (size > 0)
      {
         DWORD read = 0;
         if (!ReadFile(pipe, bytes, size, &read, nullptr) || read == 0)
         {
            return false;
         }
         bytes += read;
         size -= read;
      }
      return true;
   }

   inline std::shared_ptr<Job> ParseRequest(const std::string& request)
   {
      auto job = std::make_shared<Job>();
      size_t line_start = 0;
      bool first_line = true;
      while (line_start <= request.size())
      {
         size_t line_end = request.find('\n', line_start);
         if (line_end == std::string::npos)
         {
            line_end = request.size();
         }
         std::string_view line(request.data() + line_start, line_end - line_start);
         if (!line.empty() && line.back() == '\r')
         {
            line.remove_suffix(1);
         }
         if (first_line)
         {
            job->tool = line;
         }
         else if (const size_t equals = line.find('='); equals != std::string_view::npos)
         {
            job->args.emplace(line.substr(0, equals), line.substr(equals + 1));
         }
         first_line = false;
         line_start = line_end + 1;
      }
      return job;
   }

   // "status" answered by the pipe thread while the game doesn't present (minimized, paused, hung, or a loading screen that doesn't
   // present), when every queued tool would only time out
   inline std::optional<std::string> StatusWithoutPresents()
   {
      const uint64_t since_present_ms = GetTickCount64() - last_present_ms;
      if (since_present_ms < 1000)
      {
         return std::nullopt;
      }
      JsonWriter w;
      w.BeginObject().Field("ok", true).Field("game", game_name).Field("pid", GetCurrentProcessId()).Path("exe_path", exe_path);
      w.Field("presenting", false).Field("ms_since_present", since_present_ms).Field("frame_index", last_frame_index.load());
      {
         const std::lock_guard lock(s_mutex_jobs);
         w.Field("pending_jobs", pending_jobs.size()).Field("active_tool", active_tool);
      }
      w.Field("note", "Answered without the render thread: the game isn't presenting, the other tools would time out").EndObject();
      return std::move(w.out);
   }

   // One connected client's requests, on its own thread
   inline void ServeClient(HANDLE pipe)
   {
      connected_clients++;
      const auto write_response = [pipe](const std::string& response)
      {
         const uint32_t response_size = uint32_t(response.size());
         DWORD written = 0;
         return WriteFile(pipe, &response_size, sizeof(response_size), &written, nullptr) && WriteFile(pipe, response.data(), response_size, &written, nullptr) && written == response_size;
      };
      uint32_t size = 0;
      while (!server_stop && ReadExact(pipe, &size, sizeof(size)) && size <= (1u << 20))
      {
         std::string request(size, '\0');
         if (!ReadExact(pipe, request.data(), size))
         {
            break;
         }
         auto job = ParseRequest(request);
         if (job->tool == "status")
         {
            if (const std::optional<std::string> status = StatusWithoutPresents())
            {
               if (!write_response(*status))
               {
                  break;
               }
               continue;
            }
         }
         const DWORD timeout_ms = DWORD(job->IntArg("timeout_ms", 15000, 100, 600000)); // Mirrored by "TIMEOUT_MS" in "Scripts/luma_mcp.py"
         // Readbacks may only land inside "ReadbackRoot()": refused before the job runs, and checked again per file below (the stems come from arguments too)
         // Not "Arg()", so a tool that doesn't take it still reports it in "ignored_args"
         const auto out_dir = job->args.find("out_dir");
         const bool refused = out_dir != job->args.end() && !out_dir->second.empty() && !IsInReadbackRoot(Utf8Path(out_dir->second));
         if (!refused)
         {
            const std::lock_guard lock(s_mutex_jobs);
            pending_jobs.push_back(job);
            has_pending_jobs = true;
         }
         const ULONGLONG start = GetTickCount64();
         bool finished = false;
         while (!refused && !server_stop && !(finished = WaitForSingleObject(job->done, 50) == WAIT_OBJECT_0) && GetTickCount64() - start < timeout_ms)
         {
         }
         // A timed out job that never started is dropped (nobody reads its result), a started one finishes on its own
         bool dropped = false;
         std::string running_tool;
         if (!refused && !finished)
         {
            const std::lock_guard lock(s_mutex_jobs);
            dropped = std::erase(pending_jobs, job) != 0;
            has_pending_jobs = !pending_jobs.empty();
            running_tool = active_tool;
         }
         const auto path_error = [](std::string_view error, std::string_view key, const std::filesystem::path& path)
         {
            JsonWriter w;
            w.BeginObject().Field("ok", false).Field("error", error).Path(key, path).EndObject();
            return std::move(w.out);
         };
         const auto timeout_error = [&]
         {
            const std::string_view reason = (dropped ? "it was queued behind an earlier call that is still running, and was dropped" : "the game might not be presenting (minimized or paused?) or the job is waiting for a pass that doesn't draw");
            JsonWriter w;
            w.BeginObject().Field("ok", false).Field("error", std::format("Timed out after {} ms, {}", timeout_ms, reason));
            w.Field("ms_since_present", GetTickCount64() - last_present_ms).Field("active_tool", running_tool).EndObject();
            return std::move(w.out);
         };
         constexpr std::string_view outside_root = "out_dir must be the \"root\" folder or a subfolder of it";
         std::string response = refused ? path_error(outside_root, "root", ReadbackRoot()) : finished ? std::move(job->result.out)
                                                                                                      : timeout_error();
         // Readbacks are written here rather than on the render thread, once the job finished: a timed out one may still add files
         if (finished)
         {
            for (const auto& [path, bytes] : job->files)
            {
               if (!IsInReadbackRoot(path.parent_path()))
               {
                  response = path_error(outside_root, "root", ReadbackRoot());
                  break;
               }
               std::error_code ec;
               std::filesystem::create_directories(path.parent_path(), ec);
               std::ofstream file(path, std::ios::binary | std::ios::trunc);
               if (!file.write(bytes.data(), std::streamsize(bytes.size())))
               {
                  response = path_error("Failed to write", "path", path);
                  break;
               }
            }
         }
         if (!write_response(response))
         {
            break;
         }
      }
      connected_clients--;
      DisconnectNamedPipe(pipe);
      CloseHandle(pipe);
   }

   inline void ServerThread()
   {
      // The default pipe security of an elevated game (e.g. a "Run as administrator" compatibility flag) has a high integrity label and no
      // write access for its user, so a non elevated bridge couldn't talk to it: grant the game's user and lower the label to medium
      std::wstring sddl = L"D:P(A;;GA;;;SY)(A;;GA;;;BA)";
      HANDLE token = nullptr;
      if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
      {
         alignas(TOKEN_USER) BYTE token_user[256];
         DWORD size = 0;
         LPWSTR user_sid = nullptr;
         if (GetTokenInformation(token, TokenUser, token_user, sizeof(token_user), &size) && ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(token_user)->User.Sid, &user_sid))
         {
            sddl += std::format(L"(A;;GA;;;{})", user_sid);
            LocalFree(user_sid);
         }
         CloseHandle(token);
      }
      sddl += L"S:(ML;;NW;;;ME)";
      SECURITY_ATTRIBUTES security_attributes = {sizeof(SECURITY_ATTRIBUTES)};
      if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &security_attributes.lpSecurityDescriptor, nullptr))
      {
         security_attributes.lpSecurityDescriptor = nullptr;
      }

      struct Client
      {
         std::thread thread;
         std::atomic<bool> finished = false;
      };
      std::vector<std::shared_ptr<Client>> clients;
      const auto unfinished = [&clients]
      {
         return std::ranges::any_of(clients, [](const std::shared_ptr<Client>& client)
            { return !client->finished; });
      };
      while (!server_stop)
      {
         // One listening instance at a time, each connected client keeps its own
         HANDLE pipe = CreateNamedPipeW(pipe_name.c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, PIPE_UNLIMITED_INSTANCES, 1 << 16, 1 << 16, 0, &security_attributes);
         if (pipe == INVALID_HANDLE_VALUE)
         {
            Sleep(1000);
            continue;
         }
         if (!(ConnectNamedPipe(pipe, nullptr) || GetLastError() == ERROR_PIPE_CONNECTED) || server_stop)
         {
            DisconnectNamedPipe(pipe);
            CloseHandle(pipe);
            continue;
         }
         std::erase_if(clients, [](const std::shared_ptr<Client>& client)
            {
               if (!client->finished)
               {
                  return false;
               }
               client->thread.join();
               return true; });
         auto client = std::make_shared<Client>();
         client->thread = std::thread([client, pipe]
            {
               ServeClient(pipe);
               client->finished = true; });
         clients.push_back(std::move(client));
      }
      // Cancelled repeatedly, a client could enter a blocking read right after a cancel
      for (int i = 0; i < 150 && unfinished(); i++)
      {
         for (const auto& client : clients)
         {
            if (!client->finished)
            {
               CancelSynchronousIo(client->thread.native_handle());
            }
         }
         Sleep(10);
      }
      // Joined, as each holds a reference to the add-on module until it has exited (see "Shutdown()"). One still blocked is left
      // behind, and its reference keeps the add-on loaded.
      for (const auto& client : clients)
      {
         if (client->finished)
         {
            client->thread.join();
         }
         else
         {
            client->thread.detach();
         }
      }
      LocalFree(security_attributes.lpSecurityDescriptor);
      server_running = false;
   }

   // Starts the pipe server ("game" names the game in the replies). A second call keeps a running server, or restarts one that has exited.
   inline void Start(std::string_view game)
   {
      if (server_thread.joinable())
      {
         if (server_running)
         {
            return;
         }
         server_thread.join();
      }
      game_name = game;
      wchar_t executable[MAX_PATH] = L"";
      GetModuleFileNameW(nullptr, executable, MAX_PATH);
      exe_path = executable; // ReShade writes its log next to it by default
      pipe_name = L"\\\\.\\pipe\\luma-mcp-" + std::to_wstring(GetCurrentProcessId());
      server_stop = false;
      server_running = true;
      server_thread = std::thread(ServerThread);
   }

   // Not under the loader lock (ReShade's "AddonUninit"): a thread holds a reference to the add-on module until it has exited (the CRT's
   // thread start pins the module of its procedure), so a thread still running keeps "FreeLibrary()" from unloading the add-on, and joining
   // one at DLL_PROCESS_DETACH would deadlock on the loader lock
   inline void Shutdown()
   {
      if (!server_thread.joinable())
      {
         return;
      }
      server_stop = true;
      // Unblock the pending connection wait by connecting to ourselves; the server thread then cancels its clients' reads
      CancelSynchronousIo(server_thread.native_handle());
      HANDLE self = CreateFileW(pipe_name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
      if (self != INVALID_HANDLE_VALUE)
      {
         CloseHandle(self);
      }
      server_thread.join();
   }

   // DLL_PROCESS_DETACH: reached with the server still up only at process exit, where its threads are already gone. Detached so the
   // static "std::thread" doesn't call std::terminate() when destroyed.
   inline void Abandon()
   {
      server_stop = true;
      if (server_thread.joinable())
      {
         server_thread.detach();
      }
   }

   // Completes "active_job": its error replaces the result, the arguments no tool read are listed, and its client wakes
   inline void FinishActiveJob()
   {
      Job& job = *active_job;
      if (!job.error.empty())
      {
         job.result = {};
         job.result.BeginObject().Field("ok", false).Field("error", job.error);
      }
      // Arguments no tool read (a typo, or a key the backend doesn't know) would otherwise silently fall back to defaults
      std::vector<std::string_view> ignored_args;
      for (const auto& [key, value] : job.args)
      {
         if (!job.read_args.contains(key))
         {
            ignored_args.push_back(key);
         }
      }
      if (!ignored_args.empty())
      {
         job.result.Key("ignored_args").BeginArray();
         for (const std::string_view key : ignored_args)
         {
            job.result.Value(key);
         }
         job.result.EndArray();
      }
      job.result.EndObject();
      SetEvent(job.done);
      active_job.reset();
      const std::lock_guard lock(s_mutex_jobs);
      active_tool.clear();
   }

   // At every present, on the render thread: runs the active job, or the next queued one, through "run" (true once it is done; a
   // multi frame job returns false and runs again at the next present)
   inline void Drain(uint32_t frame_index, const std::function<bool(Job*)>& run)
   {
      last_present_ms = GetTickCount64();
      last_frame_index = frame_index;
      if (!active_job)
      {
         if (!has_pending_jobs) // Unlocked early out, every present comes through here
         {
            return;
         }
         const std::lock_guard lock(s_mutex_jobs);
         if (pending_jobs.empty())
         {
            return;
         }
         active_job = pending_jobs.front();
         pending_jobs.erase(pending_jobs.begin());
         has_pending_jobs = !pending_jobs.empty();
         active_tool = active_job->tool;
      }
      if (run(active_job.get()))
      {
         FinishActiveJob();
      }
   }
} // namespace McpPipe
