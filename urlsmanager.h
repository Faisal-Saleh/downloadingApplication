// urlmanager.h

class UrlManager {
public:
    void addUrl(const char* url, int depth);
    // getNextUrl();
    bool isEmpty() const;
    // Other methods...
private:
    // urlList;
    // Add other necessary members...
};


class UrlFileParser {
public:
    UrlFileParser(const char* filePath);
    //QList<QPair<QString, int>> parse();
private:
    const char* filePath;
};