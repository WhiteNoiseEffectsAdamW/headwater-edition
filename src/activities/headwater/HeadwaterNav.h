#pragma once
#include <string>

namespace headwater {

// In-session hint: when set, returning from a Headwater summary lands back in the
// Channels view at this channelId (the app re-opens Channels on enter) instead of
// the app menu. Reading never reboots, so an in-memory hint survives the
// Channels -> reader -> back hop. Set when a summary is opened from Channels;
// consumed (cleared) when Channels resumes; also cleared on reaching Home so a
// stale hint can't bounce a later app entry into Channels.

inline std::string& channelsResumeRef() {
  static std::string id;
  return id;
}
inline void setChannelsResume(const std::string& channelId) { channelsResumeRef() = channelId; }
inline void clearChannelsResume() { channelsResumeRef().clear(); }
inline const std::string& channelsResumeChannelId() { return channelsResumeRef(); }

}  // namespace headwater
