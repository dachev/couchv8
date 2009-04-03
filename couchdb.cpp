#include <v8.h>
#include<string>

using namespace v8;

// Extracts a C string from a V8 Utf8Value.
const char* ToCString(const String::Utf8Value& value) {
  return *value ? *value : "<string conversion failed>";
}

Handle<Value> Print(const Arguments& args) {
    bool first = true;
    for (int i = 0; i < args.Length(); i++) {
        HandleScope handle_scope;
	if (first) {
	    first = false;
	} else {
	  printf(" ");
	}
	v8::String::Utf8Value str(args[i]);
	const char* cstr = ToCString(str);
	fprintf(stdout, "%s", cstr);
    }
    fputc('\n', stdout);
    fflush(stdout);

    return Undefined();
}

Handle<Value> ReadLine(const Arguments& args) {
    std::string json;
    static const int kBufferSize = 256;
    while(true) {
        char buffer[kBufferSize];
        char* str = fgets(buffer, kBufferSize, stdin);
        int   len = strlen(str);

        if (str == NULL) {
	  break;
        }

        if (str[len-1] == '\n') {
	    json.append(str, len-1);
	    break;
        }

        json.append(str);
    }

    return v8::String::New(json.c_str(), json.size());
}

Handle<v8::String> ReadFile (const char* name) {
    FILE* file = fopen(name, "rb");
    if (file == NULL) return v8::Handle<v8::String>();

    fseek(file, 0, SEEK_END);
    int size = ftell(file);
    rewind(file);

    char* chars = new char[size + 1];
    chars[size] = '\0';
    for (int i = 0; i < size;) {
        int read = fread(&chars[i], 1, size - i, file);
        i += read;
    }
    fclose(file);
	
    int shebanger = 0;
    if (chars[0] == '#' && chars[1] == '!') {
        while (chars[shebanger] != '\n' && chars[shebanger] != '\0') shebanger ++;
    }
    v8::Handle<v8::String> result = v8::String::New(chars+shebanger, size-shebanger);
    delete[] chars;
	
    return result;
}

void ReportException(TryCatch* try_catch) {
    HandleScope handle_scope;
    String::Utf8Value exception(try_catch->Exception());
    const char* exception_string = ToCString(exception);
    Handle<Message> message = try_catch->Message();
   
    if (message.IsEmpty()) {
        // V8 didn't provide any extra information about this error; just
        // print the exception.
        printf("%s\n", exception_string);
    }
    else {
        // Print (filename):(line number): (message).
        String::Utf8Value filename(message->GetScriptResourceName());
        const char* filename_string = ToCString(filename);
        int linenum = message->GetLineNumber();
        printf("%s:%i: %s\n", filename_string, linenum, exception_string);
        // Print line of source code.
        String::Utf8Value sourceline(message->GetSourceLine());
        const char* sourceline_string = ToCString(sourceline);
        printf("%s\n", sourceline_string);
        // Print wavy underline (GetUnderline is deprecated).
        int start = message->GetStartColumn();
        for (int i = 0; i < start; i++) {
            printf(" ");
        }
        int end = message->GetEndColumn();
        for (int i = start; i < end; i++) {
            printf("^");
        }
        //printf("\n");
    }
}

// Executes a string within the current v8 context.
bool ExecuteString (Handle<String> source, Handle<Value> name, bool print_result, bool report_exceptions) {
    HandleScope handle_scope;
    TryCatch try_catch;
    Handle<Script> script = Script::Compile(source, name);
    if (script.IsEmpty()) {
        // Print errors that happened during compilation.
        if (report_exceptions)
            ReportException(&try_catch);
        return false;
    }
    else {
	Handle<Value> result = script->Run();
        if (result.IsEmpty()) {
            // Print errors that happened during execution.
	    if (report_exceptions)
                ReportException(&try_catch);
            return false;
        }
        else {
            if (print_result && !result->IsUndefined()) {
                // If all went well and the result wasn't undefined then print
                // the returned value.
                String::AsciiValue str(result);
                printf("%s\n", *str);
            }
            return true;
        }
    }
}

int main (int argc, char **argv, char **env) {
    /* Validate args */
    if (argc != 2) {
        fprintf(stderr, "incorrect number of arguments\n\n");
        fprintf(stderr, "usage: %s <scriptfile>\n", argv[0]);
        return 2;
    }

    v8::V8::SetFlagsFromCommandLine(&argc, argv, true);
    v8::HandleScope handle_scope;

    /* Create a template for the global object. */
    v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New();
    global->Set(String::New("print"), FunctionTemplate::New(Print));
    global->Set(String::New("readline"), FunctionTemplate::New(ReadLine));

    /* Create a new execution environment containing the built-in
     * functions
     */
    v8::Handle<v8::Context> context = v8::Context::New(NULL, global);
    v8::Context::Scope context_scope(context);

    /* Add spidermonkey stuff */
    v8::Handle<v8::String> spider = v8::String::New("function gc(){};");
    v8::Handle<v8::Script> scr    = v8::Script::Compile(spider);
    v8::Handle<v8::Value> result  = scr->Run();

    /* Load external JS */
    const char* str = argv[1];
    v8::Handle<v8::String> file_name = v8::String::New(str);
    v8::Handle<v8::String> source = ReadFile(str);

    if (source.IsEmpty()) {
        printf("Error reading '%s'\n", str);
        return 1;
    }

    if (!ExecuteString(source, file_name, false, true)) {
        printf("Error executing '%s'\n", str);
        return 1;
    }

    return 0;
}
