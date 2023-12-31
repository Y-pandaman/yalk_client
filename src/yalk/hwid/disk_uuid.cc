#include "yalk/hwid/disk_uuid.h"

#include <mntent.h>

#include <cstring>

#ifdef BUILD_WITH_BLKID
#include "blkid/blkid.h"
#endif

#include "yalk/hwid/disk_common.h"
#include "yalk/utils/log.h"

namespace yalk {

std::string DiskUUIDIdentifier::GetIdentifier() const {
  try {
    // Try to get the UUID of the disk where the root filesystem is located
    std::string root_fs_device = GetRootFilesystemDevice();
    std::string disk_device = GetDiskDevice(root_fs_device);
    std::string disk_uuid = GetUUIDByDiskDevice(disk_device);
    if (!disk_uuid.empty()) {
      return disk_uuid;
    }

#ifdef BUILD_WITH_BLKID
    // Try to get the UUID of the partition where the root directory is located
    std::string root_uuid = GetUUIDByMountPoint("/");
    if (!root_uuid.empty()) {
      return root_uuid;
    }
#endif

    return "";
  } catch (std::exception &e) {
    AERROR_F("Failed to get identifier for disk UUID: %s", e.what());
    return "";
  }
}

#ifdef BUILD_WITH_BLKID
std::string DiskUUIDIdentifier::GetUUIDByMountPoint(
    const std::string &mount_point) const {
  FILE *mounts;
  struct mntent *ent;
  char root_device[256];

  // Find the filesystem device for the given mount point
  mounts = setmntent("/proc/mounts", "r");
  if (mounts == nullptr) {
    perror("setmntent");
    return "";
  }

  while ((ent = getmntent(mounts)) != nullptr) {
    if (strcmp(ent->mnt_dir, mount_point.c_str()) == 0) {
      strncpy(root_device, ent->mnt_fsname, sizeof(root_device));
      break;
    }
  }
  endmntent(mounts);

  // Get the UUID of the filesystem device
  blkid_cache cache;
  blkid_dev dev;
  const char *uuid;

  if (blkid_get_cache(&cache, nullptr) != 0) {
    AERROR_F("Failed to get blkid cache");
    return "";
  }

  dev = blkid_get_dev(cache, root_device, BLKID_DEV_NORMAL);
  if (dev == nullptr) {
    AERROR_F("Failed to find device '%s'", root_device);
    return "";
  }

  uuid = blkid_get_tag_value(cache, "UUID", root_device);
  if (uuid == nullptr) {
    AERROR_F("Failed to get UUID for device '%s'", root_device);
    return "";
  }

  blkid_put_cache(cache);
  return uuid;
}
#endif

std::string DiskUUIDIdentifier::GetUUIDByDiskDevice(
    const std::string &device_path) const {
  // Get the name of the device.
  // e.g. /dev/sda -> sda
  std::string device_name =
      device_path.substr(device_path.find_last_of('/') + 1);

  // Read the WWID (World Wide Identifier) of the device.
  // This identifier is used to uniquely identify storage devices, especially in
  // storage area networks (SANs). The WWID is generated by the storage device
  // manufacturer and is guaranteed to be unique across all devices globally.
  // WWIDs are often used in multipath configurations to ensure that the correct
  // storage devices are accessed.
  std::string sysfs_path = "/sys/block/" + device_name;
  std::string wwid_path = sysfs_path + "/wwid";
  std::string wwid = ReadSysfsAttribute(wwid_path);
  if (!wwid.empty()) {
    return wwid;
  }

  // Read the UUID (Universally Unique Identifier) of the device.
  // This identifier is used to uniquely identify a filesystem or partition.
  // UUIDs are assigned to partitions when a filesystem is created, and they
  // remain constant even if the partition is moved or the disk order is
  // changed. UUIDs are particularly useful for mounting devices in the
  // /etc/fstab file, as they are more reliable than traditional device names,
  // which can change if hardware is added or removed.
  std::string uuid_path = sysfs_path + "/uuid";
  std::string uuid = ReadSysfsAttribute(uuid_path);
  if (!uuid.empty()) {
    return uuid;
  }

  return "";
}

YALK_REGISTER_HWID(DiskUUIDIdentifier);

}  // namespace yalk
