#pragma once
#include <functional>
#include <memory>
#include <se_export.hpp>
#include <string>
#include <thread.hpp>

namespace net_detail {
struct CloudState;
}

class SE_EXPORT CloudProvider {
  public:
    using VariableHandler = std::function<void(const std::string &name, const std::string &value)>;

    CloudProvider(const std::string &url, const std::string &projectId, const std::string &username,
                  const std::string &userAgent, const std::string &authToken = "");
    ~CloudProvider();

    CloudProvider(const CloudProvider &) = delete;
    CloudProvider &operator=(const CloudProvider &) = delete;

    void onVariableUpdate(VariableHandler handler);

    void start();
    void stop();

    void updateVariable(const std::string &name, const std::string &value);
    void createVariable(const std::string &name, const std::string &value);
    void renameVariable(const std::string &oldName, const std::string &newName);
    void deleteVariable(const std::string &name);

    void drainUpdates();

    bool isConnected() const;

  private:
    std::shared_ptr<net_detail::CloudState> state;
    SE_Thread thread;
    bool started = false;
};
