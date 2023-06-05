#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <regex.h>

typedef struct ScanSettingsXml {
    char* xml;
} ScanSettingsXml;

void initScanSettingsXml(ScanSettingsXml* settings, const char* s) {
    settings->xml = (char*)malloc(strlen(s) + 1);
    strcpy(settings->xml, s);
}

char* getString(const ScanSettingsXml* settings, const char* name, const char* pattern) {
    const int max_matches = 2;
    regex_t regex;
    regmatch_t matches[max_matches];

    if (regcomp(&regex, pattern, REG_EXTENDED) != 0) {
        fprintf(stderr, "Could not compile regex\n");
        exit(1);
    }

    if (regexec(&regex, settings->xml, max_matches, matches, 0) == 0) {
        size_t match_length = matches[1].rm_eo - matches[1].rm_so;
        char* result = (char*)malloc(match_length + 1);
        strncpy(result, settings->xml + matches[1].rm_so, match_length);
        result[match_length] = '\0';

        regfree(&regex);
        return result;
    }

    regfree(&regex);
    char* empty = (char*)malloc(1);
    empty[0] = '\0';
    return empty;
}

double getNumber(const ScanSettingsXml* settings, const char* name, const char* pattern) {
    char* string_value = getString(settings, name, pattern);
    double result = strtod(string_value, NULL);
    free(string_value);
    return result;
}

int main() {
    ScanSettingsXml scanSettings;
    initScanSettingsXml(&scanSettings, "<?xml version=\"1.0\" encoding=\"UTF-8\"?><scan:ScanSettings xmlns:scan=\"http://schemas.hp.com/imaging/escl/2011/05/03\" xmlns:pwg=\"http://www.pwg.org/schemas/2010/12/sm\"><pwg:Version>2.6</pwg:Version><scan:Intent>Photo</scan:Intent><pwg:ScanRegions><pwg:ScanRegion><pwg:Height>1200</pwg:Height><pwg:ContentRegionUnits>escl:ThreeHundredthsOfInches</pwg:ContentRegionUnits><pwg:Width>1800</pwg:Width><pwg:XOffset>0</pwg:XOffset><pwg:YOffset>10</pwg:YOffset></pwg:ScanRegion></pwg:ScanRegions><pwg:InputSource>Platen</pwg:InputSource><scan:ColorMode>Grayscale8</scan:ColorMode><scan:BlankPageDetection>true</scan:BlankPageDetection></scan:ScanSettings>");

    printf("XML data: %s\n", scanSettings.xml);

    char* versionPattern = "<pwg:Version>([^<]*)</pwg:Version>";
    char* version = getString(&scanSettings, "Version", versionPattern);
    printf("Version: %s\n", version);
    free(version);

    char* intentPattern = "<scan:Intent>([^<]*)</scan:Intent>";
    char* intent = getString(&scanSettings, "Intent", intentPattern);
    printf("Intent: %s\n", intent);
    free(intent);

    char* heightPattern = "<pwg:Height>([^<]*)</pwg:Height>";
    char* height = getString(&scanSettings, "Height", heightPattern);
    printf("Height: %s\n", height);
    free(height);

    char* contentRegionUnitsPattern = "<pwg:ContentRegionUnits>([^<]*)</pwg:ContentRegionUnits>";
    char* contentRegionUnits = getString(&scanSettings, "ContentRegionUnits", contentRegionUnitsPattern);
    printf("ContentRegionUnits: %s\n", contentRegionUnits);
    free(contentRegionUnits);

    char* widthPattern = "<pwg:Width>([^<]*)</pwg:Width>";
    double width = getNumber(&scanSettings, "Width", widthPattern);
    printf("Width: %.0lf\n", width);

    char* xOffsetPattern = "<pwg:XOffset>([^<]*)</pwg:XOffset>";
    double xOffset = getNumber(&scanSettings, "XOffset", xOffsetPattern);
    printf("XOffset: %.0lf\n", xOffset);

    char* yOffsetPattern = "<pwg:YOffset>([^<]*)</pwg:YOffset>";
    double yOffset = getNumber(&scanSettings, "YOffset", yOffsetPattern);
    printf("YOffset: %.0lf\n", yOffset);

    char* inputSourcePattern = "<pwg:InputSource>([^<]*)</pwg:InputSource>";
    char* inputSource = getString(&scanSettings, "InputSource", inputSourcePattern);
    printf("InputSource: %s\n", inputSource);
    free(inputSource);

    char* colorModePattern = "<scan:ColorMode>([^<]*)</scan:ColorMode>";
    char* colorMode = getString(&scanSettings, "ColorMode", colorModePattern);
    printf("ColorMode: %s\n", colorMode);
    free(colorMode);

    char* blankPageDetectionPattern = "<scan:BlankPageDetection>([^<]*)</scan:BlankPageDetection>";
    char* blankPageDetection = getString(&scanSettings, "BlankPageDetection", blankPageDetectionPattern);
    printf("BlankPageDetection: %s\n", blankPageDetection);
    free(blankPageDetection);

    free(scanSettings.xml);
    return 0;
}
