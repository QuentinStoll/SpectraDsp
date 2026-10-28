#include "MarketplaceClient.h"

#include "SettingsPath.h"

namespace Marketplace {
namespace {
class UrlHttpTransport : public HttpTransport {
   public:
    HttpResponse send(const juce::String& method,
                      const juce::String& url,
                      const juce::String& jsonBody,
                      const juce::String& bearerToken) override {
        juce::URL requestUrl(url);
        if (jsonBody.isNotEmpty()) {
            requestUrl = requestUrl.withPOSTData(jsonBody);
        }

        juce::String headers = "Accept: application/json\r\n";
        if (jsonBody.isNotEmpty()) {
            headers << "Content-Type: application/json\r\n";
        }
        if (bearerToken.isNotEmpty()) {
            headers << "Authorization: Bearer " << bearerToken << "\r\n";
        }

        int statusCode = 0;
        const auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                                 .withConnectionTimeoutMs(20000)
                                 .withExtraHeaders(headers)
                                 .withHttpRequestCmd(method)
                                 .withStatusCode(&statusCode);

        HttpResponse response;
        if (auto stream = requestUrl.createInputStream(options)) {
            stream->readIntoMemoryBlock(response.body);
        }
        response.statusCode = statusCode;
        return response;
    }
};

juce::String bodyAsString(const juce::MemoryBlock& body) {
    return juce::String::fromUTF8(static_cast<const char*>(body.getData()), static_cast<int>(body.getSize()));
}

juce::String jsonString(const juce::var& object, const juce::Identifier& key) {
    if (auto* dynamicObject = object.getDynamicObject()) {
        if (dynamicObject->hasProperty(key)) {
            const auto value = dynamicObject->getProperty(key);
            if (!value.isVoid()) {
                return value.toString();
            }
        }
    }

    return {};
}

float jsonFloat(const juce::var& object, const juce::Identifier& key, float fallback) {
    if (auto* dynamicObject = object.getDynamicObject()) {
        if (dynamicObject->hasProperty(key)) {
            const auto value = dynamicObject->getProperty(key);
            if (value.isDouble() || value.isInt() || value.isInt64()) {
                return static_cast<float>(static_cast<double>(value));
            }
        }
    }
    return fallback;
}

bool jsonBool(const juce::var& object, const juce::Identifier& key, bool fallback) {
    if (auto* dynamicObject = object.getDynamicObject()) {
        if (dynamicObject->hasProperty(key)) {
            return static_cast<bool>(dynamicObject->getProperty(key));
        }
    }

    return fallback;
}

Status statusFromCode(int statusCode, bool loginCall) {
    switch (statusCode) {
        case 200:
            return Status::Success;
        case 400:
            return Status::BadRequest;
        case 401:
            return Status::Unauthorized;
        case 403:
            return loginCall ? Status::MfaRequired : Status::Unexpected;
        case 503:
            return Status::Unavailable;
        case 0:
            return Status::TransportError;
        default:
            return Status::Unexpected;
    }
}

juce::String messageFor(Status status, bool mfaCall) {
    switch (status) {
        case Status::Unauthorized:
            return mfaCall ? "That code was refused or has expired. Sign in again."
                           : "Email or password was refused.";
        case Status::BadRequest:
            return mfaCall ? "Enter the 6-digit code."
                           : "The sign-in request is invalid.";
        case Status::Unavailable:
            return "Sign-in is unavailable right now.";
        case Status::TransportError:
            return "Could not reach profiler.audio.";
        case Status::Unexpected:
            return "profiler.audio returned an unexpected response.";
        case Status::Success:
        case Status::MfaRequired:
            break;
    }

    return {};
}

bool readSession(const juce::var& parsed, Session& session) {
    if (jsonBool(parsed, "mfaRequired", false)) {
        return false;
    }

    session.accessToken = jsonString(parsed, "accessToken");
    session.refreshToken = jsonString(parsed, "refreshToken");
    session.expiresIn = 0;
    if (auto* object = parsed.getDynamicObject()) {
        session.expiresIn = static_cast<int>(object->getProperty("expiresIn"));
    }

    if (auto* object = parsed.getDynamicObject()) {
        const auto user = object->getProperty("user");
        session.userId = jsonString(user, "id");
        session.email = jsonString(user, "email");
    }

    return session.accessToken.isNotEmpty();
}

juce::String serverErrorMessage(const juce::MemoryBlock& body) {
    const auto parsed = juce::JSON::parse(bodyAsString(body));
    return jsonString(parsed, "error");
}

LoginResult parseLoginBody(int statusCode, const juce::MemoryBlock& body, bool mfaCall) {
    LoginResult result;
    result.status = statusFromCode(statusCode, !mfaCall);
    const auto serverMessage = serverErrorMessage(body);
    result.message = serverMessage.isNotEmpty() ? serverMessage : messageFor(result.status, mfaCall);

    if (result.status == Status::TransportError || result.status == Status::Unavailable ||
        result.status == Status::BadRequest) {
        return result;
    }

    const auto parsed = juce::JSON::parse(bodyAsString(body));

    if (result.status == Status::MfaRequired) {
        result.challenge.factorId = jsonString(parsed, "factorId");
        result.challenge.challengeId = jsonString(parsed, "challengeId");
        if (result.challenge.factorId.isEmpty() || result.challenge.challengeId.isEmpty()) {
            result.status = Status::Unexpected;
            result.message = "profiler.audio did not return a verification challenge.";
            result.challenge = {};
        }
        return result;
    }

    if (result.status == Status::Success) {
        if (!readSession(parsed, result.session)) {
            result.status = Status::Unexpected;
            result.message = "The sign-in response did not include a session.";
            result.session = {};
        }
        return result;
    }

    if (mfaCall && result.status == Status::Unauthorized) {
        result.session = {};
        result.challenge = {};
    }

    return result;
}

LibraryItem parseItem(const juce::var& item) {
    LibraryItem libraryItem;
    libraryItem.entitlementId = jsonString(item, "entitlementId");
    libraryItem.acquiredAt = jsonString(item, "acquiredAt");
    libraryItem.source = jsonString(item, "source");
    libraryItem.id = jsonString(item, "id");
    libraryItem.title = jsonString(item, "title");
    libraryItem.authorName = jsonString(item, "authorName");
    libraryItem.fileUrl = jsonString(item, "fileUrl");
    libraryItem.irFileUrl = jsonString(item, "irFileUrl");
    libraryItem.hasIntegratedIr = jsonBool(item, "hasIntegratedIr", false);
    libraryItem.kind = jsonString(item, "kind");
    const auto conditionProfile = jsonString(item, "conditionProfile");
    if (conditionProfile.isNotEmpty()) {
        libraryItem.conditionProfile = conditionProfile;
    }
    libraryItem.conditionAmount = juce::jlimit(0.0f, 100.0f, jsonFloat(item, "conditionAmount", 0.0f));
    return libraryItem;
}
}  // namespace

Client::Client(std::shared_ptr<HttpTransport> transport, juce::String apiBaseUrl)
    : _transport(transport != nullptr ? std::move(transport) : std::make_shared<UrlHttpTransport>()),
      _apiBaseUrl(apiBaseUrl.trim().trimCharactersAtEnd("/")) {}

LoginResult Client::login(const juce::String& email, const juce::String& password) const {
    auto payload = std::make_unique<juce::DynamicObject>();
    payload->setProperty("email", email.trim());
    payload->setProperty("password", password);
    const auto json = juce::JSON::toString(juce::var(payload.release()), false);

    const auto response = _transport->send("POST", _apiBaseUrl + "/api/v1/plugin/login", json, {});
    return parseLoginBody(response.statusCode, response.body, false);
}

LoginResult Client::submitCode(const juce::String& factorId,
                               const juce::String& challengeId,
                               const juce::String& code) const {
    LoginResult rejected;
    if (!isSixDigitCode(code)) {
        rejected.status = Status::BadRequest;
        rejected.message = "Enter the 6-digit code.";
        return rejected;
    }

    auto payload = std::make_unique<juce::DynamicObject>();
    payload->setProperty("factorId", factorId);
    payload->setProperty("challengeId", challengeId);
    payload->setProperty("code", code);
    const auto json = juce::JSON::toString(juce::var(payload.release()), false);

    const auto response = _transport->send("POST", _apiBaseUrl + "/api/v1/plugin/mfa", json, {});
    auto result = parseLoginBody(response.statusCode, response.body, true);
    if (result.status == Status::Unauthorized) {
        result.message = "That code was refused or has expired. Sign in again.";
    } else if (result.status == Status::BadRequest) {
        result.message = "Enter the 6-digit code.";
    } else if (result.status == Status::Unavailable) {
        result.message = "Sign-in is unavailable right now.";
    }
    return result;
}

LoginResult Client::refresh(const juce::String& refreshToken) const {
    LoginResult rejected;
    if (refreshToken.isEmpty()) {
        rejected.status = Status::Unauthorized;
        rejected.message = "Your session has expired. Sign in again.";
        return rejected;
    }

    auto payload = std::make_unique<juce::DynamicObject>();
    payload->setProperty("refreshToken", refreshToken);
    const auto json = juce::JSON::toString(juce::var(payload.release()), false);
    const auto response = _transport->send("POST", _apiBaseUrl + "/api/v1/plugin/refresh", json, {});
    auto result = parseLoginBody(response.statusCode, response.body, false);
    if (result.status != Status::Success) {
        result.session = {};
        result.challenge = {};
        if (result.status == Status::Unauthorized || result.status == Status::MfaRequired) {
            result.status = Status::Unauthorized;
            result.message = "Your session has expired. Sign in again.";
        }
    }
    return result;
}

LibraryResult Client::fetchLibrary(const juce::String& accessToken) const {
    LibraryResult result;
    if (accessToken.isEmpty()) {
        result.status = Status::Unauthorized;
        result.message = "Sign in to load your marketplace library.";
        return result;
    }

    const auto response = _transport->send("GET", _apiBaseUrl + "/api/v1/plugin/library", {}, accessToken);
    result.status = statusFromCode(response.statusCode, false);

    if (result.status == Status::Unauthorized) {
        result.message = "Your session has expired. Sign in again.";
        return result;
    }

    if (result.status != Status::Success) {
        result.message = result.status == Status::TransportError
                             ? "Could not reach profiler.audio."
                             : "Could not load your marketplace library.";
        return result;
    }

    const auto parsed = juce::JSON::parse(bodyAsString(response.body));
    auto* object = parsed.getDynamicObject();
    if (object == nullptr) {
        result.status = Status::Unexpected;
        result.message = "The library response was invalid.";
        return result;
    }

    result.email = jsonString(object->getProperty("user"), "email");
    const auto library = object->getProperty("library");
    if (auto* items = library.getArray()) {
        for (const auto& item : *items) {
            result.items.push_back(parseItem(item));
        }
        return result;
    }

    result.status = Status::Unexpected;
    result.message = "The library response was invalid.";
    return result;
}

LibraryResult Client::loadLibrary(Session& session) const {
    LibraryResult result;
    if (session.accessToken.isNotEmpty()) {
        result = fetchLibrary(session.accessToken);
        if (result.status != Status::Unauthorized) {
            return result;
        }
    }

    const auto refreshed = refresh(session.refreshToken);
    if (refreshed.status != Status::Success) {
        result.status = Status::Unauthorized;
        result.items.clear();
        result.message = "Your session has expired. Sign in again.";
        return result;
    }

    session = refreshed.session;
    result = fetchLibrary(session.accessToken);
    if (result.status == Status::Unauthorized) {
        result.message = "Your session has expired. Sign in again.";
    }
    return result;
}

DownloadResult Client::download(const juce::String& fileUrl, const juce::String& accessToken) const {
    DownloadResult result;
    const auto absoluteUrl = resolveUrl(fileUrl);
    if (absoluteUrl.isEmpty()) {
        result.status = Status::BadRequest;
        result.message = "This pack has no file to download.";
        return result;
    }

    const auto token = sendBearerToken(absoluteUrl) ? accessToken : juce::String{};
    const auto response = _transport->send("GET", absoluteUrl, {}, token);
    result.status = statusFromCode(response.statusCode, false);
    if (result.status == Status::Success) {
        result.body = response.body;
        return result;
    }

    if (result.status == Status::Unauthorized) {
        result.message = "Your session has expired. Sign in again.";
    } else if (result.status == Status::TransportError) {
        result.message = "Could not reach profiler.audio.";
    } else {
        result.message = "Could not download this pack.";
    }

    return result;
}

CaptureResult Client::submitIrCapture(const juce::String& accessToken,
                                      const juce::String& title,
                                      const std::array<std::uint8_t, 32>& key,
                                      const juce::MemoryBlock& sealed) const {
    CaptureResult result;
    const auto trimmedTitle = title.trim().substring(0, 80);
    if (accessToken.isEmpty()) {
        result.status = Status::Unauthorized;
        result.message = "Sign in to clone an IR.";
        return result;
    }
    if (trimmedTitle.isEmpty() || sealed.getSize() < 36) {
        result.status = Status::BadRequest;
        result.message = "Name the IR before cloning it.";
        return result;
    }

    auto payload = std::make_unique<juce::DynamicObject>();
    payload->setProperty("title", trimmedTitle);
    payload->setProperty("key", juce::Base64::toBase64(key.data(), (int)key.size()));
    payload->setProperty("cipher", juce::Base64::toBase64(sealed.getData(), (int)sealed.getSize()));
    const auto json = juce::JSON::toString(juce::var(payload.release()), false);
    const auto response = _transport->send("POST", _apiBaseUrl + "/api/v1/plugin/captures/ir", json, accessToken);
    result.status = statusFromCode(response.statusCode, false);
    if (result.status == Status::Unauthorized) {
        result.message = "Your session has expired. Sign in again.";
        return result;
    }
    if (result.status != Status::Success) {
        const auto parsed = juce::JSON::parse(bodyAsString(response.body));
        const auto error = jsonString(parsed, "error");
        result.message = error.isNotEmpty() ? error : "Could not store the sealed IR.";
        return result;
    }

    const auto parsed = juce::JSON::parse(bodyAsString(response.body));
    result.id = jsonString(parsed, "id");
    if (result.id.isEmpty()) {
        result.status = Status::Unexpected;
        result.message = "The capture response was invalid.";
        return result;
    }
    result.message = "Saved under My creations on profiler.audio, not in purchased packs.";
    return result;
}

namespace {
AmpJobResult jobFrom(const HttpResponse& response) {
    AmpJobResult result;
    result.status = statusFromCode(response.statusCode, false);
    if (result.status != Status::Success) {
        const auto parsed = juce::JSON::parse(bodyAsString(response.body));
        const auto error = jsonString(parsed, "error");
        result.message = error.isNotEmpty() ? error : "Could not start the amp clone.";
        return result;
    }
    const auto parsed = juce::JSON::parse(bodyAsString(response.body));
    result.id = jsonString(parsed, "id");
    result.inputUploadUrl = jsonString(parsed, "inputUploadUrl");
    result.outputUploadUrl = jsonString(parsed, "outputUploadUrl");
    if (result.id.isEmpty() || result.inputUploadUrl.isEmpty() || result.outputUploadUrl.isEmpty()) {
        result.status = Status::Unexpected;
        result.message = "The amp clone response was invalid.";
    }
    return result;
}

AmpTrainResult trainFrom(const HttpResponse& response) {
    AmpTrainResult result;
    result.status = statusFromCode(response.statusCode, false);
    const auto parsed = juce::JSON::parse(bodyAsString(response.body));
    if (result.status != Status::Success) {
        const auto error = jsonString(parsed, "error");
        result.message = error.isNotEmpty() ? error : "Could not start training.";
        return result;
    }
    if (jsonString(parsed, "status") != "training") {
        result.status = Status::Unexpected;
        result.message = "The training response was invalid.";
    }
    return result;
}

AmpStatusResult statusFrom(const HttpResponse& response, bool keyCall) {
    AmpStatusResult result;
    result.status = statusFromCode(response.statusCode, false);
    const auto parsed = juce::JSON::parse(bodyAsString(response.body));
    if (result.status != Status::Success) {
        const auto error = jsonString(parsed, "error");
        result.message = error.isNotEmpty() ? error : "Could not read the amp clone.";
        return result;
    }
    result.state = jsonString(parsed, "status");
    result.fileUrl = jsonString(parsed, "fileUrl");
    result.message = jsonString(parsed, keyCall ? "key" : "message");
    if (keyCall) {
        result.message = jsonString(parsed, "key");
    }
    if (auto* object = parsed.getDynamicObject()) {
        if (object->hasProperty("conditionProfile")) {
            result.hasCondition = true;
            result.conditionProfile = jsonString(parsed, "conditionProfile");
            result.conditionAmount = juce::jlimit(0.0f, 100.0f, jsonFloat(parsed, "conditionAmount", 0.0f));
        }
    }
    return result;
}
}  // namespace

AmpJobResult Client::createAmpJob(const juce::String& accessToken, const juce::String& title) const {
    AmpJobResult result;
    if (accessToken.isEmpty()) {
        result.status = Status::Unauthorized;
        result.message = "Sign in to clone an amp.";
        return result;
    }
    auto payload = std::make_unique<juce::DynamicObject>();
    payload->setProperty("title", title.trim().substring(0, 80));
    const auto json = juce::JSON::toString(juce::var(payload.release()), false);
    return jobFrom(_transport->send("POST", _apiBaseUrl + "/api/v1/plugin/captures/amp", json, accessToken));
}

AmpTrainResult Client::startAmpTrain(const juce::String& accessToken, const juce::String& captureId) const {
    AmpTrainResult result;
    if (accessToken.isEmpty() || captureId.isEmpty()) {
        result.status = Status::BadRequest;
        result.message = "The amp clone is missing.";
        return result;
    }
    return trainFrom(_transport->send("POST", _apiBaseUrl + "/api/v1/plugin/captures/amp/" + captureId, {}, accessToken));
}

AmpStatusResult Client::ampStatus(const juce::String& accessToken, const juce::String& captureId) const {
    if (accessToken.isEmpty() || captureId.isEmpty()) {
        AmpStatusResult result;
        result.status = Status::Unauthorized;
        result.message = "Sign in to clone an amp.";
        return result;
    }
    return statusFrom(_transport->send("GET", _apiBaseUrl + "/api/v1/plugin/captures/amp/" + captureId, {}, accessToken), false);
}

AmpStatusResult Client::ampKey(const juce::String& accessToken, const juce::String& captureId) const {
    if (accessToken.isEmpty() || captureId.isEmpty()) {
        AmpStatusResult result;
        result.status = Status::Unauthorized;
        result.message = "Sign in to play this amp.";
        return result;
    }
    return statusFrom(_transport->send("GET", _apiBaseUrl + "/api/v1/plugin/captures/amp/" + captureId + "/key", {}, accessToken), true);
}

Status uploadSigned(const juce::String& signedUrl, const void* data, size_t size) {
    if (signedUrl.isEmpty() || data == nullptr || size == 0) {
        return Status::BadRequest;
    }

    juce::MemoryBlock block(data, size);
    juce::URL requestUrl(signedUrl);
    requestUrl = requestUrl.withPOSTData(block);
    int statusCode = 0;
    const auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                             .withConnectionTimeoutMs(180000)
                             .withExtraHeaders("Content-Type: application/octet-stream\r\nx-upsert: true\r\n")
                             .withHttpRequestCmd("PUT")
                             .withStatusCode(&statusCode);
    if (requestUrl.createInputStream(options) == nullptr && statusCode == 0) {
        return Status::TransportError;
    }
    return statusFromCode(statusCode, false);
}

bool Client::isSixDigitCode(const juce::String& code) {
    return code.length() == 6 && code.containsOnly("0123456789");
}

juce::String Client::resolveUrl(const juce::String& fileUrl) const {
    const auto trimmed = fileUrl.trim();
    if (trimmed.isEmpty() || trimmed == "null") {
        return {};
    }

    if (trimmed.startsWithIgnoreCase("https://") || trimmed.startsWithIgnoreCase("http://")) {
        return trimmed;
    }

    return {};
}

bool Client::sendBearerToken(const juce::String& absoluteUrl) {
    const auto host = juce::URL(absoluteUrl).getDomain();
    return host.equalsIgnoreCase("profiler.audio") || host.endsWithIgnoreCase(".profiler.audio");
}

bool saveSession(const juce::File& file, const Session& session) {
    auto user = std::make_unique<juce::DynamicObject>();
    user->setProperty("id", session.userId);
    user->setProperty("email", session.email);

    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty("accessToken", session.accessToken);
    root->setProperty("refreshToken", session.refreshToken);
    root->setProperty("expiresIn", session.expiresIn);
    root->setProperty("user", juce::var(user.release()));

    const auto parent = file.getParentDirectory();
    if (!parent.isDirectory() && !parent.createDirectory().wasOk()) {
        return false;
    }

    return file.replaceWithText(juce::JSON::toString(juce::var(root.release()), false));
}

bool loadSession(const juce::File& file, Session& session) {
    session = {};
    if (!file.existsAsFile()) {
        return false;
    }

    const auto parsed = juce::JSON::parse(file.loadFileAsString());
    if (!readSession(parsed, session)) {
        session = {};
        return false;
    }

    return true;
}

void clearSession(const juce::File& file) {
    if (file.existsAsFile()) {
        file.deleteFile();
    }
}

juce::File defaultSessionFile() {
    return getFileInSettingsFolder("marketplace_session.json");
}
}  // namespace Marketplace
