require 'FileUtils'

# Thanks! http://stackoverflow.com/questions/170956/how-can-i-find-which-operating-system-my-ruby-program-is-running-on
module OS
  def OS.windows?
    (/cygwin|mswin|mingw|bccwin|wince|emx/ =~ RUBY_PLATFORM) != nil
  end

  def OS.mac?
   (/darwin/ =~ RUBY_PLATFORM) != nil
  end

  def OS.unix?
    !OS.windows?
  end

  def OS.linux?
    OS.unix? and not OS.mac?
  end
end

module LaminaGem
  def self.dir
    File.expand_path(File.dirname(__FILE__))
  end

  def self.include_dir
    "#{self.dir}/include"
  end
end

MRuby::Gem::Specification.new('mruby-lamina') do |spec|
  spec.license = 'MIT'
  spec.author  = 'Jared Breeden'
  spec.summary = 'A Chromium shell for mruby apps'

  spec.build.cc.include_paths << "#{LaminaGem.dir}/include"
  spec.build.cxx.include_paths << "#{LaminaGem.dir}/include"
  spec.cc.flags << [ '-std=c11' ]
  spec.cxx.flags << [ '-std=c++11' ]

  if OS.mac?
    spec.cc.include_paths << "/Users/jared/projects/cef-build/cef_binary_3.2171.1979_macosx64"
    spec.cxx.include_paths << "/Users/jared/projects/cef-build/cef_binary_3.2171.1979_macosx64"
    spec.cc.include_paths << "#{LaminaGem.dir}/../mruby-cef/include"
    spec.cxx.include_paths << "#{LaminaGem.dir}/../mruby-cef/include"
    spec.linker.flags << "-F/Users/jared/projects/cef-build/cef_binary_3.2171.1979_macosx64/Release"
    spec.linker.flags << "-framework \"Chromium Embedded Framework\""
    (spec.linker.flags_after_libraries = []) << '/Users/jared/projects/cef-build/cef_binary_3.2171.1979_macosx64_build/libcef_dll/Release/libcef_dll_wrapper.a'
  elsif OS.linux?
    spec.linker.flags << '-Wl,-rpath,/opt/lamina/lib'
    spec.linker.libraries << 'X11'
  end
end
