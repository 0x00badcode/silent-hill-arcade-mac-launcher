#import <Cocoa/Cocoa.h>
#import <CommonCrypto/CommonDigest.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

static NSError *failure(NSString *message) {
    return [NSError errorWithDomain:@"ArcadeLauncher" code:1
        userInfo:@{NSLocalizedDescriptionKey:message}];
}

static NSString *digest(NSData *data) {
    unsigned char bytes[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256(data.bytes, (CC_LONG)data.length, bytes);
    NSMutableString *result = [NSMutableString string];
    for (NSUInteger i=0; i<sizeof(bytes); ++i) [result appendFormat:@"%02x", bytes[i]];
    return result;
}

static NSData *hexData(NSString *text) {
    NSMutableData *data = [NSMutableData data];
    NSString *hex = [[text componentsSeparatedByCharactersInSet:
        NSCharacterSet.whitespaceAndNewlineCharacterSet] componentsJoinedByString:@""];
    if (hex.length % 2) return nil;
    for (NSUInteger i=0; i<hex.length; i+=2) {
        unsigned value;
        NSScanner *scanner = [NSScanner scannerWithString:[hex substringWithRange:NSMakeRange(i,2)]];
        if (![scanner scanHexInt:&value] || !scanner.isAtEnd) return nil;
        unsigned char byte = value;
        [data appendBytes:&byte length:1];
    }
    return data;
}

static NSDictionary *readManifest(NSURL *resources, NSError **error) {
    NSData *data = [NSData dataWithContentsOfURL:[resources URLByAppendingPathComponent:@"GamePatch.json"]
        options:0 error:error];
    if (!data) return nil;
    return [NSJSONSerialization JSONObjectWithData:data options:0 error:error];
}

static BOOL checkLayout(NSURL *root, NSDictionary *manifest, NSError **error) {
    NSFileManager *fm = NSFileManager.defaultManager;
    NSArray *required = [manifest[@"required_files"] arrayByAddingObjectsFromArray:manifest[@"asset_files"] ?: @[]];
    for (NSString *name in required) {
        BOOL directory = NO;
        if (![fm fileExistsAtPath:[root URLByAppendingPathComponent:name].path isDirectory:&directory] || directory) {
            if (error) *error = failure([NSString stringWithFormat:@"The game folder is missing %@. Choose the complete game folder, including its Data and Shader folders.",name]);
            return NO;
        }
    }
    for (NSString *name in manifest[@"required_directories"]) {
        BOOL directory = NO;
        if (![fm fileExistsAtPath:[root URLByAppendingPathComponent:name].path isDirectory:&directory] || !directory) {
            if (error) *error = failure([NSString stringWithFormat:@"The game folder is missing %@. The executable alone is not enough to play.",name]);
            return NO;
        }
    }
    return YES;
}

static BOOL patchFile(NSURL *file, NSDictionary *spec, NSError **error) {
    NSNumber *size;
    if (![file getResourceValue:&size forKey:NSURLFileSizeKey error:error]) return NO;
    if (size.unsignedLongLongValue > 16*1024*1024) {
        if (error) *error = failure(@"This game executable is an unsupported version.");
        return NO;
    }
    NSMutableData *data = [NSMutableData dataWithContentsOfURL:file options:0 error:error];
    if (!data) return NO;
    NSString *hash = digest(data);
    if ([hash isEqualToString:spec[@"patched_sha256"]]) return YES;
    if (![hash isEqualToString:spec[@"original_sha256"]]) {
        if (error) *error = failure([NSString stringWithFormat:@"%@ is an unsupported version. This launcher supports the June 2022 standalone Windows build. Your original files have not been changed.",file.lastPathComponent]);
        return NO;
    }
    for (NSDictionary *edit in spec[@"edits"]) {
        NSData *before = hexData(edit[@"before"]), *after = hexData(edit[@"after"]);
        NSUInteger offset = [edit[@"offset"] unsignedIntegerValue];
        if (!before || !after || before.length != after.length || offset > data.length ||
            before.length > data.length-offset || memcmp((char *)data.bytes+offset,before.bytes,before.length)) {
            if (error) *error = failure(@"The compatibility patch did not match the selected game files.");
            return NO;
        }
        [data replaceBytesInRange:NSMakeRange(offset,before.length) withBytes:after.bytes];
    }
    if (![digest(data) isEqualToString:spec[@"patched_sha256"]]) {
        if (error) *error = failure(@"The compatibility patch failed its verification.");
        return NO;
    }
    return [data writeToURL:file options:NSDataWritingAtomic error:error];
}

static NSURL *findRoot(NSURL *selection) {
    NSFileManager *fm = NSFileManager.defaultManager;
    BOOL directory = NO;
    if (![fm fileExistsAtPath:selection.path isDirectory:&directory]) return nil;
    if (!directory && [selection.lastPathComponent caseInsensitiveCompare:@"KSHG.exe"]==NSOrderedSame)
        selection = selection.URLByDeletingLastPathComponent;
    if ([fm fileExistsAtPath:[selection URLByAppendingPathComponent:@"KSHG.exe"].path]) return selection;
    NSDirectoryEnumerator *items = [fm enumeratorAtURL:selection includingPropertiesForKeys:@[NSURLIsDirectoryKey]
        options:NSDirectoryEnumerationSkipsHiddenFiles|NSDirectoryEnumerationSkipsPackageDescendants errorHandler:nil];
    NSUInteger visited = 0;
    for (NSURL *item in items) {
        if (++visited > 10000) break;
        if (items.level > 5) { [items skipDescendants]; continue; }
        if ([item.lastPathComponent caseInsensitiveCompare:@"KSHG.exe"]==NSOrderedSame)
            return item.URLByDeletingLastPathComponent;
    }
    return nil;
}

static BOOL isGameArchive(NSURL *file) {
    NSString *extension = file.pathExtension.lowercaseString;
    return [extension isEqualToString:@"zip"] || [extension isEqualToString:@"rar"];
}

static NSURL *archiveInFolder(NSURL *folder, NSError **error) {
    NSNumber *directory = nil;
    if (![folder getResourceValue:&directory forKey:NSURLIsDirectoryKey error:error] || !directory.boolValue)
        return nil;
    NSArray<NSURL *> *items = [NSFileManager.defaultManager contentsOfDirectoryAtURL:folder
        includingPropertiesForKeys:@[NSURLIsRegularFileKey]
        options:NSDirectoryEnumerationSkipsHiddenFiles error:error];
    if (!items) return nil;
    NSURL *archive = nil;
    for (NSURL *item in items) {
        if (!isGameArchive(item)) continue;
        NSNumber *regular = nil;
        if (![item getResourceValue:&regular forKey:NSURLIsRegularFileKey error:error]) return nil;
        if (!regular.boolValue) continue;
        if (archive) {
            if (error) *error = failure(@"This folder contains more than one ZIP or RAR. Select the game archive itself.");
            return nil;
        }
        archive = item;
    }
    return archive;
}

static BOOL rejectLinks(NSURL *root, NSError **error) {
    NSNumber *linked;
    if (![root getResourceValue:&linked forKey:NSURLIsSymbolicLinkKey error:error]) return NO;
    if (linked.boolValue) {
        if (error) *error = failure(@"Choose a complete copy of the game files, rather than a folder of links.");
        return NO;
    }
    __block NSError *enumerationError = nil;
    NSDirectoryEnumerator *items = [NSFileManager.defaultManager enumeratorAtURL:root
        includingPropertiesForKeys:@[NSURLIsSymbolicLinkKey] options:0 errorHandler:^BOOL(NSURL *url,NSError *problem) {
            (void)url;
            enumerationError = problem;
            return NO;
        }];
    for (NSURL *item in items) {
        if (![item getResourceValue:&linked forKey:NSURLIsSymbolicLinkKey error:error]) return NO;
        if (linked.boolValue) {
            if (error) *error = failure(@"The game files contain a symbolic link. Choose an extracted, complete Windows game folder.");
            return NO;
        }
    }
    if (enumerationError && error) *error = enumerationError;
    return enumerationError == nil;
}

static BOOL importGame(NSURL *selection, NSURL *state, NSURL *resources, NSError **error) {
    NSFileManager *fm = NSFileManager.defaultManager;
    NSDictionary *manifest = readManifest(resources,error);
    if (!manifest) return NO;
    NSURL *temporary = [state URLByAppendingPathComponent:[@".import-" stringByAppendingString:NSUUID.UUID.UUIDString]];
    if (![fm createDirectoryAtURL:temporary withIntermediateDirectories:YES attributes:nil error:error]) return NO;
    BOOL success = NO;
    NSURL *root = selection;
    NSURL *staging = [temporary URLByAppendingPathComponent:@"Game"];
    @try {
        NSURL *archive = isGameArchive(selection) ? selection : nil;
        if (!archive) {
            root = findRoot(selection);
            if (!root) {
                archive = archiveInFolder(selection,error);
                if (!archive && error && *error) return NO;
            }
        }
        if (archive) {
            root = [temporary URLByAppendingPathComponent:@"Extracted"];
            if (![fm createDirectoryAtURL:root withIntermediateDirectories:YES attributes:nil error:error]) return NO;
            NSTask *extract = [NSTask new];
            BOOL rar = [archive.pathExtension.lowercaseString isEqualToString:@"rar"];
            extract.executableURL = [NSURL fileURLWithPath:rar ? @"/usr/bin/bsdtar" : @"/usr/bin/ditto"];
            extract.arguments = rar ? @[@"-xf",archive.path,@"-C",root.path]
                                    : @[@"-x",@"-k",archive.path,root.path];
            extract.standardInput = NSFileHandle.fileHandleWithNullDevice;
            extract.standardOutput = NSFileHandle.fileHandleWithNullDevice;
            extract.standardError = NSFileHandle.fileHandleWithNullDevice;
            if (![extract launchAndReturnError:error]) return NO;
            [extract waitUntilExit];
            if (extract.terminationStatus) {
                if (error) *error = failure(@"The archive could not be extracted. Check that it is complete and not password-protected, or extract it first and choose its game folder.");
                return NO;
            }
        }
        root = root ? findRoot(root) : nil;
        if (!root) {
            if (error) *error = failure(@"No KSHG.exe was found. Choose the complete game folder, its KSHG.exe, a ZIP or RAR, or a folder containing one game archive.");
            return NO;
        }
        if (!checkLayout(root,manifest,error) || !rejectLinks(root,error)) return NO;
        if (![fm createDirectoryAtURL:staging withIntermediateDirectories:YES attributes:nil error:error]) return NO;
        for (NSString *name in manifest[@"required_files"]) {
            if (![fm copyItemAtURL:[root URLByAppendingPathComponent:name]
                toURL:[staging URLByAppendingPathComponent:name] error:error]) return NO;
        }
        for (NSString *name in manifest[@"files"])
            if (!patchFile([staging URLByAppendingPathComponent:name],manifest[@"files"][name],error)) return NO;
        for (NSString *name in @[@"Data",@"Shader"]) {
            if (![fm copyItemAtURL:[root URLByAppendingPathComponent:name]
                toURL:[staging URLByAppendingPathComponent:name] error:error]) return NO;
        }
        if (![fm copyItemAtURL:[resources URLByAppendingPathComponent:@"Support/macd.dll"]
            toURL:[staging URLByAppendingPathComponent:@"macd.dll"] error:error]) return NO;
        NSURL *destination = [state URLByAppendingPathComponent:@"Game"];
        BOOL existed = [fm fileExistsAtPath:destination.path];
        if (existed) {
            if (renamex_np(staging.fileSystemRepresentation,destination.fileSystemRepresentation,RENAME_SWAP)) {
                if (error) *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:errno userInfo:nil];
                return NO;
            }
        } else if (![fm moveItemAtURL:staging toURL:destination error:error]) {
            return NO;
        }
        success = YES;
    } @finally {
        [fm removeItemAtURL:temporary error:nil];
    }
    return success;
}

static BOOL isInstalled(NSURL *state, NSURL *resources) {
    NSDictionary *manifest = readManifest(resources,nil);
    NSURL *game = [state URLByAppendingPathComponent:@"Game"];
    if (!manifest || !checkLayout(game,manifest,nil)) return NO;
    for (NSString *name in manifest[@"files"]) {
        NSData *data = [NSData dataWithContentsOfURL:[game URLByAppendingPathComponent:name]];
        if (!data || ![digest(data) isEqualToString:manifest[@"files"][name][@"patched_sha256"]]) return NO;
    }
    return YES;
}

static void initializeApplication(void) {
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        [NSApplication.sharedApplication setActivationPolicy:NSApplicationActivationPolicyAccessory];
        [NSApplication.sharedApplication finishLaunching];
    });
}

static void activate(void) {
    initializeApplication();
    [NSApplication.sharedApplication setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApplication.sharedApplication activateIgnoringOtherApps:YES];
}

static int showError(NSString *details, NSURL *log) {
    activate();
    NSAlert *alert = [NSAlert new];
    alert.messageText = @"Silent Hill couldn’t start";
    alert.informativeText = details;
    [alert addButtonWithTitle:@"Close"];
    if (log) [alert addButtonWithTitle:@"Show Log"];
    if ([alert runModal]==NSAlertSecondButtonReturn)
        [NSWorkspace.sharedWorkspace activateFileViewerSelectingURLs:@[log]];
    return 1;
}

static NSWindow *progressWindow(NSString *message) {
    activate();
    NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0,0,430,130)
        styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO];
    window.title = @"Silent Hill The Arcade";
    window.releasedWhenClosed = NO;
    NSTextField *label = [NSTextField wrappingLabelWithString:message];
    label.frame = NSMakeRect(24,61,382,45);
    [window.contentView addSubview:label];
    NSProgressIndicator *spinner = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(24,28,382,16)];
    spinner.indeterminate = YES;
    [spinner startAnimation:nil];
    [window.contentView addSubview:spinner];
    [window center];
    [window makeKeyAndOrderFront:nil];
    return window;
}

int main(int argc, const char **argv) {
    @autoreleasepool {
        NSFileManager *fm = NSFileManager.defaultManager;
        NSURL *resources = NSBundle.mainBundle.resourceURL;
        NSURL *support = [fm URLsForDirectory:NSApplicationSupportDirectory inDomains:NSUserDomainMask].firstObject;
        NSURL *state = [support URLByAppendingPathComponent:@"Silent Hill The Arcade"];
        const char *override = getenv("SILENT_HILL_STATE");
        if (override && override[0]=='/') state = [NSURL fileURLWithPath:@(override)];
        BOOL cli = argc==3 && strcmp(argv[1],"--import")==0;
        if (!cli) initializeApplication();
        NSError *error = nil;
        if (![fm createDirectoryAtURL:state withIntermediateDirectories:YES attributes:nil error:&error])
            return cli ? (fprintf(stderr,"%s\n",error.localizedDescription.UTF8String),1) : showError(error.localizedDescription,nil);
        int lock = open([state URLByAppendingPathComponent:@"launch.lock"].fileSystemRepresentation,O_CREAT|O_RDWR|O_CLOEXEC,0600);
        if (lock<0) return cli ? 1 : showError(@"The local settings folder could not be opened.",nil);
        if (flock(lock,LOCK_EX|LOCK_NB)) { close(lock); return cli ? 2 : 0; }
        if (cli) {
            BOOL result = importGame([NSURL fileURLWithPath:@(argv[2])],state,resources,&error);
            fprintf(result ? stdout : stderr,"%s\n",result ? "Game imported and verified." : error.localizedDescription.UTF8String);
            close(lock);
            return result ? 0 : 1;
        }
        if (!isInstalled(state,resources)) {
            activate();
            NSOpenPanel *panel = [NSOpenPanel openPanel];
            panel.title = @"Choose your Silent Hill game files";
            panel.message = @"Choose your game folder, KSHG.exe, or a ZIP or RAR archive. You can also choose a folder containing one game archive.";
            panel.prompt = @"Import Game";
            panel.canChooseDirectories = YES;
            panel.canChooseFiles = YES;
            panel.allowsMultipleSelection = NO;
            panel.allowedContentTypes = @[UTTypeFolder, UTTypeZIP,
                [UTType typeWithFilenameExtension:@"rar"],
                [UTType typeWithFilenameExtension:@"exe"]];
            if ([panel runModal]!=NSModalResponseOK) { close(lock); return 0; }
            NSWindow *progress = progressWindow(@"Importing your game files. This may take a few minutes.");
            __block BOOL done=NO, imported=NO;
            __block NSError *importError=nil;
            NSURL *selection = panel.URL;
            dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0),^{
                @autoreleasepool {
                    NSError *problem=nil;
                    BOOL result=importGame(selection,state,resources,&problem);
                    dispatch_async(dispatch_get_main_queue(),^{ imported=result; importError=problem; done=YES; });
                }
            });
            while (!done) [NSRunLoop.mainRunLoop runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
            [progress close];
            if (!imported) { close(lock); return showError(importError.localizedDescription,nil); }
        }
        NSURL *adapter = [state URLByAppendingPathComponent:@"Game/macd.dll"];
        [fm removeItemAtURL:adapter error:nil];
        if (![fm copyItemAtURL:[resources URLByAppendingPathComponent:@"Support/macd.dll"] toURL:adapter error:&error]) {
            close(lock); return showError(error.localizedDescription,nil);
        }
        NSURL *log = [state URLByAppendingPathComponent:@"last-launch.log"];
        if (![fm createFileAtPath:log.path contents:nil attributes:nil]) {
            close(lock); return showError(@"The launch log could not be created.",nil);
        }
        NSFileHandle *output = [NSFileHandle fileHandleForWritingAtPath:log.path];
        NSTask *game = [NSTask new];
        game.executableURL = [NSURL fileURLWithPath:@"/bin/bash"];
        game.arguments = @[[resources URLByAppendingPathComponent:@"play.sh"].path,state.path];
        NSDictionary *host = NSProcessInfo.processInfo.environment;
        NSMutableDictionary *environment = [NSMutableDictionary dictionary];
        for (NSString *name in @[@"HOME",@"USER",@"LOGNAME",@"TMPDIR",@"LANG",@"LC_ALL"])
            if (host[name]) environment[name]=host[name];
        game.environment = environment;
        game.standardOutput = output;
        game.standardError = output;
        if (![game launchAndReturnError:&error]) { close(lock); return showError(error.localizedDescription,log); }
        NSWindow *progress = nil;
        if (![fm fileExistsAtPath:[state URLByAppendingPathComponent:@"ready"].path])
            progress = progressWindow(@"Preparing the first launch. This can take a minute or two.");
        while (game.running) {
            if (progress) {
                NSString *text = [NSString stringWithContentsOfURL:log encoding:NSUTF8StringEncoding error:nil];
                if ([text containsString:@"Starting Silent Hill"]) { [progress close]; progress=nil; }
            }
            [NSRunLoop.mainRunLoop runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];
        }
        [progress close];
        [game waitUntilExit];
        [output closeFile];
        close(lock);
        if (game.terminationStatus) return showError(@"The game exited during launch or play. If Rosetta is missing, install it before trying again. The launch log has the details.",log);
        return 0;
    }
}
