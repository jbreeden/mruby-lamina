module Lamina
  class << self
    # Configurable options
    attr_accessor :browser_ipc_path
    attr_accessor :cache_path
    attr_accessor :remote_debugging_port
    attr_accessor :server_port
    attr_accessor :script_v8_extensions
    attr_accessor :url
    attr_accessor :use_page_titles
    attr_accessor :window_title
  end

  # Configurable options
  @browser_ipc_path = "ipc://#{(1..20).map { |i| ('a'.ord + rand(25)).chr }.join}/browser.ipc"
  @cache_path = nil
  @server_port = nil
  @script_v8_extensions = "./lamina_v8_extensions.rb"
  @remote_debugging_port = 8888
  @url = ""
  @use_page_titles = false
  @window_title = "Lamina"

  # Internal-only variables
  @lock_file_path = ".lamina"
  @options_file_path = ".lamina_options"
  @lock_file = nil

  def self.run
    ensure_lock_file_exists
    # TODO: Place this call correctly
    #set_localhost_storage_file_port
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
      # Don't set @lock_file variable until opened for reading (when we get the lock)
      File.open(@lock_file_path, 'w') do |lock_file|
        lock_file.puts "LAMINA LOCK FILE"
      end
    end
  end

  def self.determine_launch_mode
    # Not closing this file because the shared lock
    # should be maintained as long as this app is running
    puts "Opening lock file"
    @lock_file = File.open(@lock_file_path, 'r')
    if @lock_file.flock(File::LOCK_EX | File::LOCK_NB)
      puts "Got exclusive lock on lock file. App is being launched by user"
      :launching
    elsif  ENV['CEF_SUBPROC']
      puts "CEF_SUBPROC environment variable was found. This is a CEF sub process"
      :cef_process
    else
      puts "App is being relaunched by user"
      :relaunching
    end
  end

  def self.on_launch(&block)
    @on_launch_proc = block
  end

  def self.launch
    puts "Switching to shared lock"
    @lock_file.flock(File::LOCK_UN)
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

    puts "Starting browser message server"
    start_browser_message_server
    puts "Starting CEF"
    start_cef_proc
  end

  def self.on_relaunch(&block)
    @on_relaunch_block = block
  end

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
      puts "Lamina.url must be a string matching %r[(ipc|tcp)://.*]"
      exit 1
    end

    unless @cache_path.nil? || Dir.exists?(@cache_path)
      puts "If Lamina.cache_path is specified, it must be an existing directory"
      exit 1
    end

    unless @server_port.nil? || @server_port.kind_of?(Fixnum)
      puts "If Lamina.cache_path is supplied, it must be an int"
      exit 1
    end

    # If the default is left, but the file doesn't exist, lamina will just
    # skip loading it. If the client has specified a different value, we
    # should probably warn them if the file doesn't exist
    unless @script_v8_extensions == "./lamina_v8_extensions.rb" || File.exists?(@script_v8_extensions)
      puts "Lamina.script_v8_extensions was specifed as #{@script_v8_extensions} but this file does not exist"
      exit 1
    end

    unless @remote_debugging_port.nil? || @remote_debugging_port.kind_of?(Fixnum)
      puts "If Lamina.remoute_debugging_port is specified, it must be an int"
      exit 1
    end

    unless @url =~ %r[(https?|file)://.*]
      puts "Lamina.url must be a string matching %r[(https?|file)://.*]"
      exit 1
    end

    # @use_page_titles can be anything, if the user doesn't set nil
    # or false it will evaluate as true... no biggy, so no validation

    unless @window_title.nil? || @window_title.kind_of?(String)
      puts "If Lamina.window_title is specified, it must be a string"
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

    if Dir.exists? "#{@cache_path}/Local Storage"
      Dir.chdir("#{@cache_path}/Local Storage") do
        Dir.entries('.').each do |f|
          if m = f.match(/^http_localhost_([0-9]*).localstorage/i)
            File.rename f, f.sub(/localhost_([0-9]*)/, @cache_path)
          end
        end
      end
    end
  end

end
