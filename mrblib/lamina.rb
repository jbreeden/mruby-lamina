#<
# # Module Lamina
#>
module Lamina
  class << self
    #<
    # ## `attr_accessor :browser_ipc_path`
    # - Get or set the `browser_ipc_path` (must be a string).
    # - This tells lamina threads/processes how to communicate with the main browser thread.
    # - If you don't know why you'd want to set this, don't worry about it.
    # - Default value: A radomly generated url (that you should probably just leave alone).
    #>
    attr_accessor :browser_ipc_path

    #<
    # ## `attr_accessor :cache_path`
    # - Get or set the `cache_path` option.
    # - This tells lamina where to store cache data from the browser,
    #   such as localstorage and the like.
    # - If the specified path does not exists, lamina will create the directory.
    # - The parent of the indicated path must exist already.
    # - Default value: `nil` (No cache data will be saved)
    #>
    attr_accessor :cache_path

    #<
    # ## `attr_accessor :remote_debugging_port`
    # - Get or set the `remote_debugging_port` option (must be an int).
    # - This tells lamina what port (if any) to open for remote debugging
    #   via chrome dev tools.
    # - If specified, after launching the application you can navigate to
    #   `http://localhost:#{remote_debugging_port}` in a chrome window to
    #   access chrom dev tools for you app, inspect the html, and run javascript.
    # - Default value: `0` (disable remote debugging)
    #>
    attr_accessor :remote_debugging_port

    #<
    # ## `attr_accessor :server_port`
    # - Get or set the `server_port` option (must be an int)
    # - This tells lamina what port you're running your app server on.
    # - If you're app does not use a server, or you're server is not on localhost,
    #   this option has no effect.
    # - If you're running a server on localhost as part of your application, this
    #   option allows lamina to adjust the localstorage file names each time
    #   the app is launched so that previously set localstorage data can be accessed -
    #   even though the origin of the site (which includes the port) has actually changed.
    # - Default value: `nil`
    #>
    attr_accessor :server_port

    #<
    # ## `attr_accessor :script_v8_extensions`
    # - Get or set the `script_v8_extensions` option (must be a string)
    # - This tells lamina what file to load when a new V8 context is created.
    # - In this file, you can use the [`mruby-cef`](https://github.com/jbreeden/mruby-cef/blob/master/doc/src/mruby_cef_v8.md)
    #   API to write javascript extension for use in your application. (See `javascript_interop` sample for examples).
    # - If this file does not exist, no error will be reported. It is simply ignored.
    # - Default value: `./lamina_v8_extensions.rb`
    #>
    attr_accessor :script_v8_extensions

    #<
    # ## `attr_accessor :url`
    # - Get or set the `url` option (must be a string)
    # - This tells lamina what url to load on app launch
    # - Accepts `file://`, `http://`, and `https://` urls
    # - Default value: `nil` (This is required however, you _must_ overwrite the default)
    #>
    attr_accessor :url

    #<
    # ## `attr_accessor :use_page_titles`
    # - Get or set the `use_page_titles` option (boolean)
    # - This tells lamina whether to update the title bar with the page title specified in
    #   any loaded page.
    # - Default value: `false` (The `window_title` option will be used for the liftime of the app)
    #>
    attr_accessor :use_page_titles

    #<
    # ## `attr_accessor :window_title`
    # - Get or set the `window_title` option (must be a string)
    # - This tells lamina what text to display in the application's title bar
    # - Default value: `Lamina`
    #>
    attr_accessor :window_title
  end

  # Configurable options
  @browser_ipc_path = "ipc://#{(1..20).map { |i| ('a'.ord + rand(25)).chr }.join}/browser.ipc"
  @cache_path = nil
  @server_port = nil
  @script_v8_extensions = "./lamina_v8_extensions.rb"
  @remote_debugging_port = 0
  @url = nil
  @use_page_titles = false
  @window_title = "Lamina"

  # Internal-only variables
  @lock_file_path = ".lamina"
  @options_file_path = ".lamina_options"
  @lock_file = nil

  def self.run
    ensure_lock_file_exists
    launch_mode = determine_launch_mode
    case launch_mode
    when :launching
      launch
    when :cef_process
      read_lamina_options
      start_cef_proc
    when :relaunching
      read_lamina_options
      relaunch
    end
  end

  def self.ensure_lock_file_exists
    unless File.exists?(@lock_file_path)
      puts "Creating lock file"
      # Don't set @lock_file variable until opened for reading (when we get the lock)
      File.open(@lock_file_path, 'w') do |lock_file|
        lock_file.puts "LAMINA LOCK FILE"
      end
    end
  end

  def self.determine_launch_mode
    puts "Determining launch mode"
    
    # - Not closing this file because the shared lock
    # should be maintained as long as this app is running.
    # - On linux, file must be open for writing to get an exclusive lock,
    # so opening with 'a' mode
    @lock_file = File.open(@lock_file_path, 'a')
    # Child processes may get redundant exclusive locks due to file descriptors being shared.
    # So, check if this is a subprocess first. (TODO: Double check this)
    if ENV['CEF_SUBPROC']
      puts "CEF_SUBPROC environment variable was found. This is a CEF sub process"
      :cef_process
    elsif @lock_file.flock(File::LOCK_EX | File::LOCK_NB)
      puts "Got exclusive lock on lock file. App is being launched by user"
      :launching
    else
      puts "App is being relaunched by user"
      :relaunching
    end
  end

  #<
  # ## `::on_launch(&block)`
  # - Provide a block to lamina specifying launch behavior.
  # - The provided block is only called when the app is started, and no existing
  #   instances are running.
  # - This is where you should specify the application options such as the url to load, etc.
  #   + Options are set by using the `attr_accessor`s described above.
  #   + Options set in the `on_launch` block are automatically loaded if another
  #     app instance is launched (see: `::on_relaunch`)
  #>
  def self.on_launch(&block)
    @on_launch_proc = block
  end

  def self.launch
    puts "Switching to shared lock"
    @lock_file.flock(File::LOCK_SH)

    if @on_launch_proc
      puts "Running on_launch callback"
      @on_launch_proc[]
      puts "Writing options to #{@options_file_path}"
      File.open(@options_file_path, 'w') do |opt_file|
        print_options(opt_file)
      end
      puts "Validating options"
      validate_options
    end
    puts "Launching options:"
    print_options($stdout, '  ')

    set_localhost_storage_file_port
    puts "Starting browser message server"
    start_browser_message_server
    puts "Starting CEF"
    start_cef_proc
  end

  #<
  # ## `::on_relaunch(&block)`
  # - Provide a block to lamina specifying relaunch behavior.
  # - The provided block is only called when the app is started, but an instance
  #   is already running.
  # - The lamina options (such as `url`, `window_title`, etc.) set for the running
  #   application instance are re-used automatically
  #>
  def self.on_relaunch(&block)
    @on_relaunch_block = block
  end

  #<
  # ## `::open_new_window`
  # - Should only be called in the `on_relaunch` block.
  # - Opens a new window for the application.
  # - Same as running `window.open('/');` in the existing browser window.
  #>
  # (This method is defined in mruby_lamina.cpp, but documented here to keep docs in one file)

  def self.relaunch
    File.open(@options_file_path, 'r') do |f|
      puts "Relaunching lamina with options:"
      print_options($stdout, '  ')
      @on_relaunch_block[] if @on_relaunch_block
    end
  end

  def self.read_lamina_options
    File.open(@options_file_path, 'r') do |f|
      puts "Reading options from #{@options_file_path}"
      eval f.read
    end
  end

  def self.print_options(file, indent = '')
    file.puts "#{indent}Lamina.browser_ipc_path = #{ @browser_ipc_path ? "'#{@browser_ipc_path}'" : 'nil' }"
    file.puts "#{indent}Lamina.cache_path = #{ @cache_path ? "'#{@cache_path}'" : 'nil' }"
    file.puts "#{indent}Lamina.remote_debugging_port = #{@remote_debugging_port || 'nil' }"
    file.puts "#{indent}Lamina.server_port = #{@server_port || 'nil' }"
    file.puts "#{indent}Lamina.script_v8_extensions = #{ @script_v8_extensions ? "'#{@script_v8_extensions}'" : 'nil' }"
    file.puts "#{indent}Lamina.url = #{ @url ? "'#{@url}'" : 'nil' }"
    file.puts "#{indent}Lamina.use_page_titles = #{@use_page_titles || 'false' }"
    file.puts "#{indent}Lamina.window_title = #{ @window_title ? "'#{@window_title}'" : 'nil' }"
  end

  def self.validate_options
    unless @browser_ipc_path =~ %r[(ipc|tcp)://.*]
      puts "Erro: Lamina.url must be a string matching %r[(ipc|tcp)://.*]"
      exit 1
    end

    unless @cache_path.nil? || Dir.exists?(@cache_path)
      begin
        Dir.mkdir @cache_path
      rescue Exception => ex
        puts "Erro: Lamina.cache_path (#{@cache_path}), directory does not exist and could not be created."
        puts ex
        exit 1
      end
    end

    unless @server_port.nil? || @server_port.kind_of?(Fixnum)
      puts "Erro: If Lamina.cache_path is supplied, it must be an int"
      exit 1
    end

    # If the default is left, but the file doesn't exist, lamina will just
    # skip loading it. If the client has specified a different value, we
    # should probably warn them if the file doesn't exist
    unless @script_v8_extensions == "./lamina_v8_extensions.rb" || File.exists?(@script_v8_extensions)
      puts "Erro: Lamina.script_v8_extensions was specifed as #{@script_v8_extensions} but this file does not exist"
      exit 1
    end

    unless @remote_debugging_port.nil? || @remote_debugging_port.kind_of?(Fixnum)
      puts "Erro: If Lamina.remoute_debugging_port is specified, it must be an int"
      exit 1
    end

    unless @url =~ %r[(https?|file)://.*]
      puts "Erro: Lamina.url must be a string matching %r[(https?|file)://.*]"
      exit 1
    end

    # @use_page_titles can be anything, if the user doesn't set nil
    # or false it will evaluate as true... no biggy, so no validation

    unless @window_title.nil? || @window_title.kind_of?(String)
      puts "Erro: If Lamina.window_title is specified, it must be a string"
    end
  end

  # If running a server on localhost, the localstorage files
  # are named and looked up according to the port number. If
  # different port numbers are used each time the app is started,
  # the localstorage files will not be found by CEF so localstorage
  # will not work. Renaming the files by the current port in use
  # solves this.
  def self.set_localhost_storage_file_port
    return if @cache_path.nil? || @server_port.nil?

    puts "Updating localstorage files"

    if Dir.exists? "#{@cache_path}/Local Storage"
      Dir.chdir("#{@cache_path}/Local Storage") do
        Dir.entries('.').each do |f|
          if m = f.match(/^http_localhost_([0-9]*).localstorage/i)
            puts "Renaming #{f} to #{f.sub(/localhost_([0-9]*)/, @server_port.to_s)}"
            File.rename f, f.sub(/localhost_([0-9]*)/, "localhost_#{@server_port}")
          end
        end
      end
    end
  end

end
