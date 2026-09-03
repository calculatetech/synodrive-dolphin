#include "synodrive_compatibility.h"

#include <elf.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr std::string_view iconActivatedName =
    "_ZN7SysTray13iconActivatedEN15QSystemTrayIcon16ActivationReasonE";
constexpr std::string_view showStyledMenuName = "_ZN7SysTray14showStyledMenuEv";
constexpr std::string_view launchPreferenceName = "_ZN7SysTray19sigLaunchPreferenceEv";
constexpr std::string_view raiseWizardName = "_ZN7SysTray14sigRaiseWizardEv";
constexpr std::size_t iconActivatedSize = 96;

constexpr std::array<std::uint8_t, iconActivatedSize> baseTemplate = {
    0x55, 0x48, 0x89, 0xe5, 0x48, 0x83, 0xec, 0x10,
    0x48, 0x89, 0x7d, 0xf8, 0x89, 0x75, 0xf4, 0x8b,
    0x45, 0xf4, 0x83, 0xf8, 0x03, 0x74, 0x3f, 0x83,
    0xf8, 0x04, 0x74, 0x3d, 0x83, 0xf8, 0x02, 0x74,
    0x02, 0xeb, 0x3a, 0x48, 0x8b, 0x45, 0xf8, 0x8b,
    0x40, 0x38, 0x83, 0xf8, 0x01, 0x75, 0x0e, 0x48,
    0x8b, 0x45, 0xf8, 0x48, 0x89, 0xc7, 0xe8, 0x00,
    0x00, 0x00, 0x00, 0xeb, 0x1f, 0x48, 0x8b, 0x45,
    0xf8, 0x8b, 0x40, 0x38, 0x85, 0xc0, 0x75, 0x14,
    0x48, 0x8b, 0x45, 0xf8, 0x48, 0x89, 0xc7, 0xe8,
    0x00, 0x00, 0x00, 0x00, 0xeb, 0x06, 0x90, 0xeb,
    0x04, 0x90, 0xeb, 0x01, 0x90, 0x90, 0xc9, 0xc3,
};

struct PostCommitError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct Snapshot {
    std::vector<std::uint8_t> bytes;
    struct stat metadata {};
};

struct Symbol {
    Elf64_Sym value {};
    Elf64_Shdr section {};
};

enum class State { Unpatched, Patched };

std::runtime_error systemError(std::string_view action) {
    return std::runtime_error(std::string(action) + ": " + std::strerror(errno));
}

bool inRange(std::uint64_t offset, std::uint64_t size, std::size_t total) {
    return offset <= total && size <= total - offset;
}

template<typename T>
T readObject(const std::vector<std::uint8_t>& bytes, std::uint64_t offset) {
    if (!inRange(offset, sizeof(T), bytes.size())) {
        throw std::runtime_error("truncated ELF data");
    }
    T result;
    std::memcpy(&result, bytes.data() + offset, sizeof(result));
    return result;
}

std::string_view readString(const std::vector<std::uint8_t>& bytes,
                            const Elf64_Shdr& table,
                            std::uint32_t offset) {
    if (offset >= table.sh_size || !inRange(table.sh_offset, table.sh_size, bytes.size())) {
        throw std::runtime_error("invalid ELF string table");
    }
    const char* first = reinterpret_cast<const char*>(bytes.data() + table.sh_offset + offset);
    const std::size_t remaining = table.sh_size - offset;
    const void* terminator = std::memchr(first, '\0', remaining);
    if (!terminator) {
        throw std::runtime_error("unterminated ELF string");
    }
    return {first, static_cast<std::size_t>(static_cast<const char*>(terminator) - first)};
}

std::int32_t relativeDisplacement(std::uint64_t target, std::uint64_t next) {
    const auto distance = static_cast<__int128>(target) - static_cast<__int128>(next);
    if (distance < std::numeric_limits<std::int32_t>::min()
        || distance > std::numeric_limits<std::int32_t>::max()) {
        throw std::runtime_error("tray function jump is out of range");
    }
    return static_cast<std::int32_t>(distance);
}

void writeI32(std::array<std::uint8_t, iconActivatedSize>& bytes,
              std::size_t offset,
              std::int32_t value) {
    static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__);
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

class TrayImage {
public:
    explicit TrayImage(const std::vector<std::uint8_t>& bytes) : bytes_(bytes) {
        const Elf64_Ehdr header = readObject<Elf64_Ehdr>(bytes_, 0);
        if (std::memcmp(header.e_ident, ELFMAG, SELFMAG) != 0
            || header.e_ident[EI_CLASS] != ELFCLASS64
            || header.e_ident[EI_DATA] != ELFDATA2LSB
            || header.e_ident[EI_VERSION] != EV_CURRENT
            || header.e_version != EV_CURRENT
            || header.e_type != ET_DYN
            || header.e_machine != EM_X86_64
            || header.e_ehsize != sizeof(Elf64_Ehdr)
            || header.e_shentsize != sizeof(Elf64_Shdr)
            || header.e_shnum == 0
            || header.e_shstrndx == SHN_XINDEX
            || header.e_shstrndx >= header.e_shnum) {
            throw std::runtime_error("unsupported Synology Drive executable format");
        }
        if (!inRange(header.e_shoff,
                     static_cast<std::uint64_t>(header.e_shnum) * sizeof(Elf64_Shdr),
                     bytes_.size())) {
            throw std::runtime_error("truncated ELF section table");
        }

        sections_.reserve(header.e_shnum);
        for (std::size_t index = 0; index < header.e_shnum; ++index) {
            sections_.push_back(readObject<Elf64_Shdr>(
                bytes_, header.e_shoff + index * sizeof(Elf64_Shdr)));
        }
        const Elf64_Shdr& sectionNames = sections_[header.e_shstrndx];
        if (sectionNames.sh_type != SHT_STRTAB) {
            throw std::runtime_error("invalid ELF section-name table");
        }

        std::size_t symtabIndex = sections_.size();
        for (std::size_t index = 0; index < sections_.size(); ++index) {
            if (sections_[index].sh_type == SHT_SYMTAB
                && readString(bytes_, sectionNames, sections_[index].sh_name) == ".symtab") {
                if (symtabIndex != sections_.size()) {
                    throw std::runtime_error("duplicate ELF symbol table");
                }
                symtabIndex = index;
            }
        }
        if (symtabIndex == sections_.size()) {
            throw std::runtime_error("missing ELF symbol table");
        }

        const Elf64_Shdr& symtab = sections_[symtabIndex];
        if (symtab.sh_entsize != sizeof(Elf64_Sym)
            || symtab.sh_size % sizeof(Elf64_Sym) != 0
            || symtab.sh_link >= sections_.size()
            || sections_[symtab.sh_link].sh_type != SHT_STRTAB
            || !inRange(symtab.sh_offset, symtab.sh_size, bytes_.size())) {
            throw std::runtime_error("invalid ELF symbol table");
        }
        const Elf64_Shdr& strings = sections_[symtab.sh_link];

        const std::array<std::string_view, 4> names = {
            iconActivatedName, showStyledMenuName, launchPreferenceName, raiseWizardName,
        };
        std::array<std::vector<Symbol>, names.size()> matches;
        for (std::size_t offset = 0; offset < symtab.sh_size; offset += sizeof(Elf64_Sym)) {
            const Elf64_Sym symbol = readObject<Elf64_Sym>(bytes_, symtab.sh_offset + offset);
            const std::string_view name = readString(bytes_, strings, symbol.st_name);
            for (std::size_t index = 0; index < names.size(); ++index) {
                if (name == names[index]) {
                    if (ELF64_ST_TYPE(symbol.st_info) != STT_FUNC
                        || symbol.st_shndx == SHN_XINDEX
                        || symbol.st_shndx >= sections_.size()) {
                        throw std::runtime_error("invalid tray function symbol");
                    }
                    const Elf64_Shdr& section = sections_[symbol.st_shndx];
                    if (section.sh_type != SHT_PROGBITS
                        || (section.sh_flags & SHF_EXECINSTR) == 0
                        || symbol.st_value < section.sh_addr
                        || symbol.st_size > section.sh_size
                        || symbol.st_value - section.sh_addr > section.sh_size - symbol.st_size) {
                        throw std::runtime_error("tray function is outside executable data");
                    }
                    matches[index].push_back({symbol, section});
                }
            }
        }
        for (const auto& match : matches) {
            if (match.size() != 1) {
                throw std::runtime_error("missing or duplicate tray function symbol");
            }
        }

        icon_ = matches[0].front();
        show_ = matches[1].front();
        launch_ = matches[2].front();
        raise_ = matches[3].front();
        if (icon_.value.st_size != iconActivatedSize
            || show_.value.st_size != 134
            || launch_.value.st_size != 44
            || raise_.value.st_size != 44) {
            throw std::runtime_error("unsupported tray function size");
        }
        iconOffset_ = fileOffset(icon_);
        if (!inRange(iconOffset_, iconActivatedSize, bytes_.size())) {
            throw std::runtime_error("truncated tray activation handler");
        }

        original_ = baseTemplate;
        writeI32(original_, 55, relativeDisplacement(
            launch_.value.st_value, icon_.value.st_value + 59));
        writeI32(original_, 80, relativeDisplacement(
            raise_.value.st_value, icon_.value.st_value + 84));
        patched_ = original_;
        patched_[27] = 0x42;
        patched_[86] = 0xc9;
        patched_[87] = 0xe9;
        writeI32(patched_, 88, relativeDisplacement(
            show_.value.st_value, icon_.value.st_value + 92));
    }

    State state() const {
        const auto* first = bytes_.data() + iconOffset_;
        if (std::equal(original_.begin(), original_.end(), first)) {
            return State::Unpatched;
        }
        if (std::equal(patched_.begin(), patched_.end(), first)) {
            return State::Patched;
        }
        throw std::runtime_error("unsupported tray activation handler");
    }

    std::vector<std::uint8_t> replacement(State target) const {
        std::vector<std::uint8_t> result = bytes_;
        const auto& function = target == State::Patched ? patched_ : original_;
        std::copy(function.begin(), function.end(), result.begin() + iconOffset_);
        return result;
    }

private:
    std::uint64_t fileOffset(const Symbol& symbol) const {
        const std::uint64_t within = symbol.value.st_value - symbol.section.sh_addr;
        if (!inRange(symbol.section.sh_offset, symbol.section.sh_size, bytes_.size())
            || !inRange(symbol.section.sh_offset + within,
                        symbol.value.st_size,
                        bytes_.size())) {
            throw std::runtime_error("invalid tray function file offset");
        }
        return symbol.section.sh_offset + within;
    }

    const std::vector<std::uint8_t>& bytes_;
    std::vector<Elf64_Shdr> sections_;
    Symbol icon_;
    Symbol show_;
    Symbol launch_;
    Symbol raise_;
    std::uint64_t iconOffset_ = 0;
    std::array<std::uint8_t, iconActivatedSize> original_ {};
    std::array<std::uint8_t, iconActivatedSize> patched_ {};
};

Snapshot readTarget(const fs::path& path) {
    struct stat linkMetadata {};
    if (::lstat(path.c_str(), &linkMetadata) != 0) {
        throw systemError("cannot inspect Synology Drive executable");
    }
    if (!S_ISREG(linkMetadata.st_mode)) {
        throw std::runtime_error("Synology Drive executable is not a regular file");
    }

    const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) {
        throw systemError("cannot open Synology Drive executable");
    }
    Snapshot result;
    if (::fstat(fd, &result.metadata) != 0) {
        const int saved = errno;
        ::close(fd);
        errno = saved;
        throw systemError("cannot inspect open Synology Drive executable");
    }
    if (!S_ISREG(result.metadata.st_mode)
        || result.metadata.st_dev != linkMetadata.st_dev
        || result.metadata.st_ino != linkMetadata.st_ino
        || result.metadata.st_size < 0
        || static_cast<std::uintmax_t>(result.metadata.st_size)
            > std::numeric_limits<std::size_t>::max()) {
        ::close(fd);
        throw std::runtime_error("Synology Drive executable changed while opening it");
    }

    result.bytes.resize(static_cast<std::size_t>(result.metadata.st_size));
    std::size_t offset = 0;
    while (offset < result.bytes.size()) {
        const ssize_t count = ::read(fd, result.bytes.data() + offset, result.bytes.size() - offset);
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            const int saved = count < 0 ? errno : EIO;
            ::close(fd);
            errno = saved;
            throw systemError("cannot read Synology Drive executable");
        }
        offset += static_cast<std::size_t>(count);
    }
    if (::close(fd) != 0) {
        throw systemError("cannot close Synology Drive executable");
    }
    return result;
}

#ifdef SYNODRIVE_TRAY_PATCH_TESTING
bool inject(std::string_view point) {
    const char* value = std::getenv("SYNODRIVE_TRAY_PATCH_TEST_FAIL");
    return value && value == point;
}
#else
bool inject(std::string_view) {
    return false;
}
#endif

void writeAll(int fd, const std::vector<std::uint8_t>& bytes) {
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const ssize_t count = ::write(fd, bytes.data() + offset, bytes.size() - offset);
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            throw systemError("cannot write replacement executable");
        }
        offset += static_cast<std::size_t>(count);
    }
}

void replaceTarget(const fs::path& path,
                   const Snapshot& original,
                   const std::vector<std::uint8_t>& replacement) {
    const fs::path directory = path.parent_path();
    const int directoryFd = ::open(directory.c_str(), O_RDONLY | O_CLOEXEC | O_DIRECTORY);
    if (directoryFd < 0) {
        throw systemError("cannot open Synology Drive directory");
    }

    std::string temporary = path.string() + ".synodrive-dolphin.XXXXXX";
    std::vector<char> name(temporary.begin(), temporary.end());
    name.push_back('\0');
    int fd = ::mkstemp(name.data());
    if (fd < 0) {
        const int saved = errno;
        ::close(directoryFd);
        errno = saved;
        throw systemError("cannot create replacement executable");
    }
    const fs::path temporaryPath(name.data());
    bool committed = false;
    try {
        struct stat temporaryMetadata {};
        if (::fstat(fd, &temporaryMetadata) != 0) {
            throw systemError("cannot inspect replacement executable");
        }
        if ((temporaryMetadata.st_uid != original.metadata.st_uid
             || temporaryMetadata.st_gid != original.metadata.st_gid)
            && ::fchown(fd, original.metadata.st_uid, original.metadata.st_gid) != 0) {
            throw systemError("cannot preserve executable ownership");
        }
        if (::fchmod(fd, original.metadata.st_mode & 07777) != 0) {
            throw systemError("cannot preserve executable mode");
        }
        if (inject("write")) {
            errno = EIO;
            throw systemError("cannot write replacement executable");
        }
        writeAll(fd, replacement);
        if (::fsync(fd) != 0) {
            throw systemError("cannot synchronize replacement executable");
        }
        if (::close(fd) != 0) {
            fd = -1;
            throw systemError("cannot close replacement executable");
        }
        fd = -1;

        if (inject("target-change")) {
            const int targetFd = ::open(path.c_str(), O_WRONLY | O_CLOEXEC | O_NOFOLLOW);
            std::uint8_t changed = original.bytes.front() ^ 1;
            if (targetFd < 0) {
                throw systemError("cannot inject concurrent target change");
            }
            if (::pwrite(targetFd, &changed, 1, 0) != 1) {
                const int saved = errno;
                ::close(targetFd);
                errno = saved;
                throw systemError("cannot inject concurrent target change");
            }
            if (::close(targetFd) != 0) {
                throw systemError("cannot inject concurrent target change");
            }
        }
        const Snapshot current = readTarget(path);
        if (current.metadata.st_dev != original.metadata.st_dev
            || current.metadata.st_ino != original.metadata.st_ino
            || current.bytes != original.bytes) {
            throw std::runtime_error("Synology Drive executable changed before replacement");
        }
        if (inject("rename")) {
            errno = EIO;
            throw systemError("cannot replace Synology Drive executable");
        }
        if (::rename(temporaryPath.c_str(), path.c_str()) != 0) {
            throw systemError("cannot replace Synology Drive executable");
        }
        committed = true;
        if (inject("directory-fsync") || ::fsync(directoryFd) != 0) {
            throw PostCommitError(
                "target changed, but directory synchronization failed; run status before retrying");
        }
        ::close(directoryFd);
    } catch (...) {
        if (fd >= 0) {
            ::close(fd);
        }
        if (!committed) {
            ::unlink(temporaryPath.c_str());
        }
        ::close(directoryFd);
        throw;
    }
}

fs::path homeDirectory() {
    const char* value = std::getenv("HOME");
    if (!value || !*value || !fs::path(value).is_absolute()) {
        throw std::runtime_error("HOME must be a nonempty absolute path");
    }
    return value;
}

const char* stateName(State state) {
    return state == State::Patched ? "patched" : "unpatched";
}

}

int main(int argc, char** argv) {
    if (argc != 2
        || (std::string_view(argv[1]) != "status"
            && std::string_view(argv[1]) != "apply"
            && std::string_view(argv[1]) != "restore")) {
        std::cerr << "usage: synodrive-tray-patch status|apply|restore\n";
        return 2;
    }

    try {
        const fs::path root = homeDirectory() / ".SynologyDrive/SynologyDrive.app";
        if (!synodrive::supportedInstalledVersion(root / "INFO")) {
            throw std::runtime_error("unsupported Synology Drive Client version");
        }
        const fs::path target = root / "bin/cloud-drive-ui";
        const Snapshot snapshot = readTarget(target);
        const TrayImage image(snapshot.bytes);
        const State current = image.state();
        const std::string_view command(argv[1]);
        if (command == "status") {
            std::cout << stateName(current) << '\n';
            return 0;
        }

        const State requested = command == "apply" ? State::Patched : State::Unpatched;
        if (current != requested) {
            replaceTarget(target, snapshot, image.replacement(requested));
        }
        std::cout << stateName(requested) << '\n';
        if (current != requested) {
            if (requested == State::Patched) {
                std::cerr << "synodrive-tray-patch: restart Synology Drive Client to load the tray patch\n";
            } else {
                std::cerr << "synodrive-tray-patch: restart Synology Drive Client to remove the tray patch\n";
            }
        }
        return 0;
    } catch (const PostCommitError& error) {
        std::cerr << "synodrive-tray-patch: " << error.what() << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "synodrive-tray-patch: " << error.what() << '\n';
        return 1;
    }
}
