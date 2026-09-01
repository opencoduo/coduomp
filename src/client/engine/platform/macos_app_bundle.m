#import <AppKit/AppKit.h>

#include "macos_app_bundle.h"

#include <string.h>

enum {
    CODUOMP_MACOS_PATH_CAPACITY = 4096,
    CODUOMP_MACOS_STEAM_SEARCH_DEPTH = 5
};

static const char *const coduompMacOSDataPathPreference =
    "OpenCoDUORetailDataPath";
static char coduompMacOSDataPath[CODUOMP_MACOS_PATH_CAPACITY];
static char coduompMacOSResourcesPath[CODUOMP_MACOS_PATH_CAPACITY];
static char coduompMacOSFrameworksPath[CODUOMP_MACOS_PATH_CAPACITY];
static int coduompMacOSBundlePathsInitialized;
static int coduompMacOSIsApplicationBundle;

/* NOT_FROM_ORIGINAL_SOURCE: copy a Foundation filesystem representation into
 * the engine's stable C-string storage without publishing a partial path. */
static int coduomp_macos_copy_url_path(NSURL *url, char *destination,
                                      size_t destinationSize)
{
    const char *path;
    size_t length;

    if (url == nil || destination == NULL || destinationSize == 0)
        return 0;

    path = url.fileSystemRepresentation;
    if (path == NULL)
        return 0;
    length = strlen(path);
    if (length >= destinationSize)
        return 0;

    memcpy(destination, path, length + 1u);
    return 1;
}

/* NOT_FROM_ORIGINAL_SOURCE: identify a real Finder-launchable application
 * bundle. Command-line development builds retain their existing cwd paths. */
static int coduomp_macos_initialize_bundle_paths(void)
{
    NSBundle *bundle;
    NSString *bundlePath;

    if (coduompMacOSBundlePathsInitialized != 0)
        return coduompMacOSIsApplicationBundle;

    coduompMacOSBundlePathsInitialized = 1;
    bundle = NSBundle.mainBundle;
    bundlePath = bundle.bundlePath;
    if (bundlePath == nil ||
        [bundlePath.pathExtension caseInsensitiveCompare:@"app"] !=
            NSOrderedSame) {
        return 0;
    }

    if (coduomp_macos_copy_url_path(bundle.resourceURL,
                                    coduompMacOSResourcesPath,
                                    sizeof(coduompMacOSResourcesPath)) == 0 ||
        coduomp_macos_copy_url_path(bundle.privateFrameworksURL,
                                    coduompMacOSFrameworksPath,
                                    sizeof(coduompMacOSFrameworksPath)) == 0) {
        coduompMacOSResourcesPath[0] = '\0';
        coduompMacOSFrameworksPath[0] = '\0';
        return 0;
    }

    coduompMacOSIsApplicationBundle = 1;
    return 1;
}

/* NOT_FROM_ORIGINAL_SOURCE: locate a direct child without assuming that a
 * user-owned installation resides on a case-insensitive volume. */
static NSURL *coduomp_macos_child_url(NSURL *directory, NSString *name,
                                     int requireDirectory)
{
    NSFileManager *fileManager = NSFileManager.defaultManager;
    NSArray<NSURL *> *children =
        [fileManager contentsOfDirectoryAtURL:directory
                   includingPropertiesForKeys:nil
                                      options:NSDirectoryEnumerationSkipsHiddenFiles
                                        error:nil];

    for (NSURL *child in children) {
        BOOL isDirectory = NO;
        NSURL *resolvedChild;

        if ([child.lastPathComponent caseInsensitiveCompare:name] !=
            NSOrderedSame) {
            continue;
        }
        resolvedChild = child.URLByResolvingSymlinksInPath;
        if (![fileManager fileExistsAtPath:resolvedChild.path
                               isDirectory:&isDirectory]) {
            continue;
        }
        if ((requireDirectory != 0) != (isDirectory != NO))
            continue;
        return resolvedChild;
    }
    return nil;
}

/* NOT_FROM_ORIGINAL_SOURCE: a distributable engine accepts only a complete
 * base-game plus United Offensive data root. Directory names are matched
 * case-insensitively, while canonical retail PK3s prevent false positives. */
static NSURL *coduomp_macos_validate_data_root(NSURL *candidate)
{
    NSFileManager *fileManager = NSFileManager.defaultManager;
    BOOL isDirectory = NO;
    NSURL *root;
    NSURL *mainDirectory;
    NSURL *uoDirectory;

    if (candidate == nil)
        return nil;

    root = candidate.URLByStandardizingPath.URLByResolvingSymlinksInPath;
    if (![fileManager fileExistsAtPath:root.path
                           isDirectory:&isDirectory] || !isDirectory) {
        return nil;
    }

    if ([root.lastPathComponent caseInsensitiveCompare:@"main"] ==
            NSOrderedSame ||
        [root.lastPathComponent caseInsensitiveCompare:@"uo"] ==
            NSOrderedSame) {
        root = root.URLByDeletingLastPathComponent;
    }

    mainDirectory = coduomp_macos_child_url(root, @"main", 1);
    uoDirectory = coduomp_macos_child_url(root, @"uo", 1);
    if (mainDirectory == nil || uoDirectory == nil)
        return nil;
    if (coduomp_macos_child_url(mainDirectory, @"pak0.pk3", 0) == nil ||
        coduomp_macos_child_url(uoDirectory, @"pakuo00.pk3", 0) == nil) {
        return nil;
    }
    return root;
}

/* NOT_FROM_ORIGINAL_SOURCE: bounded directory walk for the few wrapper
 * layouts used by Steam and legacy macOS application bundles. */
static NSURL *coduomp_macos_find_data_root(NSURL *candidate,
                                          NSUInteger remainingDepth,
                                          NSMutableSet<NSString *> *visited)
{
    NSFileManager *fileManager = NSFileManager.defaultManager;
    NSURL *root = coduomp_macos_validate_data_root(candidate);
    NSString *canonicalPath;
    NSArray<NSURL *> *children;

    if (root != nil)
        return root;
    if (candidate == nil || remainingDepth == 0)
        return nil;

    canonicalPath = candidate.URLByStandardizingPath
                              .URLByResolvingSymlinksInPath.path;
    if (canonicalPath == nil || [visited containsObject:canonicalPath])
        return nil;
    [visited addObject:canonicalPath];

    children = [fileManager contentsOfDirectoryAtURL:candidate
                          includingPropertiesForKeys:nil
                                             options:NSDirectoryEnumerationSkipsHiddenFiles
                                               error:nil];
    for (NSURL *child in children) {
        BOOL isDirectory = NO;
        NSURL *found;

        if (![fileManager fileExistsAtPath:child.path
                               isDirectory:&isDirectory] || !isDirectory) {
            continue;
        }
        if ([child.lastPathComponent caseInsensitiveCompare:@"main"] ==
                NSOrderedSame ||
            [child.lastPathComponent caseInsensitiveCompare:@"uo"] ==
                NSOrderedSame) {
            continue;
        }
        found = coduomp_macos_find_data_root(
            child, remainingDepth - 1u, visited);
        if (found != nil)
            return found;
    }
    return nil;
}

/* NOT_FROM_ORIGINAL_SOURCE: read quoted scalar values from Steam's small VDF
 * manifests without introducing a Steam SDK or general-purpose parser. */
static NSArray<NSString *> *coduomp_macos_vdf_values(NSString *contents,
                                                     NSString *key)
{
    NSString *escapedKey;
    NSString *pattern;
    NSRegularExpression *expression;
    NSArray<NSTextCheckingResult *> *matches;
    NSMutableArray<NSString *> *values = [NSMutableArray array];

    if (contents == nil)
        return values;
    escapedKey = [NSRegularExpression escapedPatternForString:key];
    pattern = [NSString stringWithFormat:@"\"%@\"\\s+\"([^\"]+)\"",
                                         escapedKey];
    expression = [NSRegularExpression regularExpressionWithPattern:pattern
                                                            options:0
                                                              error:nil];
    matches = [expression matchesInString:contents
                                  options:0
                                    range:NSMakeRange(0, contents.length)];
    for (NSTextCheckingResult *match in matches) {
        NSString *value;

        if (match.numberOfRanges < 2)
            continue;
        value = [contents substringWithRange:[match rangeAtIndex:1]];
        value = [value stringByReplacingOccurrencesOfString:@"\\\\"
                                                 withString:@"\\"];
        [values addObject:value];
    }
    return values;
}

/* NOT_FROM_ORIGINAL_SOURCE: discover Steam app 2640 across the default and
 * configured library folders, then resolve any legacy app-wrapper nesting. */
static NSURL *coduomp_macos_find_steam_data(void)
{
    NSFileManager *fileManager = NSFileManager.defaultManager;
    NSURL *steamRoot =
        [fileManager.homeDirectoryForCurrentUser
            URLByAppendingPathComponent:
                @"Library/Application Support/Steam"
                           isDirectory:YES];
    NSURL *libraryFoldersURL =
        [steamRoot URLByAppendingPathComponent:@"steamapps/libraryfolders.vdf"
                                   isDirectory:NO];
    NSString *libraryFolders =
        [NSString stringWithContentsOfURL:libraryFoldersURL
                                 encoding:NSUTF8StringEncoding
                                    error:nil];
    NSMutableArray<NSString *> *libraryPaths = [NSMutableArray array];

    [libraryPaths addObject:steamRoot.path];
    for (NSString *path in coduomp_macos_vdf_values(libraryFolders, @"path")) {
        if (![libraryPaths containsObject:path])
            [libraryPaths addObject:path];
    }

    for (NSString *libraryPath in libraryPaths) {
        NSURL *libraryURL = [NSURL fileURLWithPath:libraryPath
                                      isDirectory:YES];
        NSURL *manifestURL =
            [libraryURL URLByAppendingPathComponent:
                            @"steamapps/appmanifest_2640.acf"
                                         isDirectory:NO];
        NSString *manifest =
            [NSString stringWithContentsOfURL:manifestURL
                                     encoding:NSUTF8StringEncoding
                                        error:nil];
        NSString *installDirectory =
            coduomp_macos_vdf_values(manifest, @"installdir").firstObject;
        NSURL *candidate;
        NSURL *found;

        if (installDirectory == nil)
            continue;
        candidate = [[[libraryURL URLByAppendingPathComponent:@"steamapps"
                                                   isDirectory:YES]
            URLByAppendingPathComponent:@"common"
                             isDirectory:YES]
            URLByAppendingPathComponent:installDirectory
                             isDirectory:YES];
        found = coduomp_macos_find_data_root(
            candidate, CODUOMP_MACOS_STEAM_SEARCH_DEPTH,
            [NSMutableSet set]);
        if (found != nil)
            return found;
    }
    return nil;
}

/* NOT_FROM_ORIGINAL_SOURCE: check a short list of conventional legacy Mac
 * install names without recursively scanning the user's Applications tree. */
static NSURL *coduomp_macos_find_conventional_data(void)
{
    NSFileManager *fileManager = NSFileManager.defaultManager;
    NSArray<NSURL *> *applicationRoots = @[
        [NSURL fileURLWithPath:@"/Applications" isDirectory:YES],
        [fileManager.homeDirectoryForCurrentUser
            URLByAppendingPathComponent:@"Applications" isDirectory:YES]
    ];
    NSArray<NSString *> *names = @[
        @"Call of Duty United Offensive",
        @"Call of Duty United Offensive.app",
        @"Call of Duty - United Offensive",
        @"Call of Duty - United Offensive.app"
    ];

    for (NSURL *applicationRoot in applicationRoots) {
        for (NSString *name in names) {
            NSURL *candidate =
                [applicationRoot URLByAppendingPathComponent:name
                                                 isDirectory:YES];
            NSURL *found = coduomp_macos_find_data_root(
                candidate, CODUOMP_MACOS_STEAM_SEARCH_DEPTH,
                [NSMutableSet set]);
            if (found != nil)
                return found;
        }
    }
    return nil;
}

/* NOT_FROM_ORIGINAL_SOURCE: present a native first-launch folder picker and
 * keep prompting after an incomplete base-only or expansion-only selection. */
static NSURL *coduomp_macos_choose_data_root(void)
{
    NSOpenPanel *panel = NSOpenPanel.openPanel;
    NSURL *steamCommon =
        [NSFileManager.defaultManager.homeDirectoryForCurrentUser
            URLByAppendingPathComponent:
                @"Library/Application Support/Steam/steamapps/common"
                           isDirectory:YES];

    panel.canChooseFiles = NO;
    panel.canChooseDirectories = YES;
    panel.allowsMultipleSelection = NO;
    panel.canCreateDirectories = NO;
    panel.prompt = @"Use Game Data";
    panel.title = @"Locate Call of Duty: United Offensive";
    panel.message =
        @"Choose the installation folder containing both the main and uo "
         "folders. OpenCoDUO reads your existing game data in place and does "
         "not copy it into the application.";
    if ([NSFileManager.defaultManager fileExistsAtPath:steamCommon.path])
        panel.directoryURL = steamCommon;

    for (;;) {
        NSURL *root;
        NSAlert *alert;

        [NSApp activateIgnoringOtherApps:YES];
        if ([panel runModal] != NSModalResponseOK)
            return nil;
        root = coduomp_macos_validate_data_root(panel.URL);
        if (root != nil)
            return root;

        alert = [[NSAlert alloc] init];
        alert.alertStyle = NSAlertStyleWarning;
        alert.messageText = @"That folder is not a complete installation";
        alert.informativeText =
            @"Select the parent folder containing both main/pak0.pk3 and "
             "uo/pakuo00.pk3.";
        [alert addButtonWithTitle:@"Choose Another Folder"];
        [alert runModal];
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: recognize the engine's ordinary +set override so
 * command-line launches never display Finder onboarding UI. */
static int coduomp_macos_has_command_line_cd_path(int argc,
                                                  char *const argv[])
{
    for (int index = 1; index + 2 < argc; ++index) {
        if (strcmp(argv[index], "+set") == 0 &&
            strcmp(argv[index + 1], "fs_cdpath") == 0 &&
            argv[index + 2][0] != '\0') {
            return 1;
        }
    }
    return 0;
}

/* NOT_FROM_ORIGINAL_SOURCE: resolve and persist the user-owned retail data
 * root before the recovered engine initializes its filesystem cvars. */
int coduomp_macos_prepare_launch(int argc, char *const argv[])
{
    @autoreleasepool {
        NSUserDefaults *defaults;
        NSString *savedPath;
        NSURL *dataRoot = nil;

        if (coduomp_macos_initialize_bundle_paths() == 0)
            return 1;

        /* Cocoa's accent chooser must never claim held movement keys.  This
         * is set before first-run onboarding creates an NSApplication, which
         * can precede SDL's own registration of the same application default. */
        defaults = NSUserDefaults.standardUserDefaults;
        [defaults setBool:NO forKey:@"ApplePressAndHoldEnabled"];

        if (coduomp_macos_has_command_line_cd_path(argc, argv) != 0)
            return 1;

        savedPath = [defaults stringForKey:
            [NSString stringWithUTF8String:coduompMacOSDataPathPreference]];
        if (savedPath != nil) {
            dataRoot = coduomp_macos_validate_data_root(
                [NSURL fileURLWithPath:savedPath isDirectory:YES]);
        }
        if (dataRoot == nil)
            dataRoot = coduomp_macos_find_steam_data();
        if (dataRoot == nil)
            dataRoot = coduomp_macos_find_conventional_data();
        if (dataRoot == nil) {
            (void)NSApplication.sharedApplication;
            dataRoot = coduomp_macos_choose_data_root();
        }
        if (dataRoot == nil)
            return 0;
        if (coduomp_macos_copy_url_path(dataRoot, coduompMacOSDataPath,
                                        sizeof(coduompMacOSDataPath)) == 0) {
            return 0;
        }

        [defaults setObject:dataRoot.path
                     forKey:[NSString stringWithUTF8String:
                         coduompMacOSDataPathPreference]];
        return 1;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: expose the stable selected retail-data path. */
const char *coduomp_macos_default_cd_path(void)
{
    return coduompMacOSDataPath;
}

/* NOT_FROM_ORIGINAL_SOURCE: expose the app's immutable Resources directory. */
const char *coduomp_macos_bundle_resources_path(void)
{
    if (coduomp_macos_initialize_bundle_paths() == 0)
        return NULL;
    return coduompMacOSResourcesPath;
}

/* NOT_FROM_ORIGINAL_SOURCE: form a reconstructed-module path within the app's
 * private Frameworks directory. */
int coduomp_macos_framework_module_path(const char *moduleName,
                                        char *path, size_t pathSize)
{
    @autoreleasepool {
        NSURL *frameworksURL;
        NSURL *moduleURL;

        if (moduleName == NULL ||
            coduomp_macos_initialize_bundle_paths() == 0) {
            return 0;
        }
        frameworksURL = [NSURL fileURLWithPath:
            [NSString stringWithUTF8String:coduompMacOSFrameworksPath]
                                    isDirectory:YES];
        moduleURL = [frameworksURL
            URLByAppendingPathComponent:
                [NSString stringWithUTF8String:moduleName]
                             isDirectory:NO];
        return coduomp_macos_copy_url_path(moduleURL, path, pathSize);
    }
}
