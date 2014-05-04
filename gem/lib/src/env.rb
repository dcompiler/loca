require 'yaml'  # for serialization

module Labenv
  # paths for the files and directories
  LocaDir = ".loca"
  ConfigFile = "config.yaml"
  DataDir = "data"
  @@paths = nil # root directory of Locality Lab

  # commands: base and configured
  Commands = [:config, :run, :upload, :uninstall, :help]
  @@env = nil

  # user inputs
  PinDir = "pin directory"
  LocaServer = "loca server"
  UserInputs = [PinDir, LocaServer]
 
  # debugging
  Debug = true

  def self.env
    read_config if @@env == nil
    return @@env
  end

  def self.paths
    setup_paths if @@paths == nil
    return @@paths
  end

  def self.config( gui=true )
    
    if Labenv.env == nil
      @@env = Hash.new  # initial env
      Labenv.env[PinDir] = '/localdisk/cding/pin/pin-2.9-39599-gcc.3.4.6-ia32_intel64-linux'
      Labenv.env[LocaServer] = Labenv.paths[Labenv::DataDir] # 'cycle1.cs.rochester.edu:/p/compiler/loclab/data'
    end

    if gui
      inputs_gui( @@env )
    else
      inputs_cmdln( @@env )
    end
    
    save_env

    require 'src/external'
    LabExternal.add_footprint_analyzer  # Pin tool and compile
    LabExternal.add_anytaskset_analyzer  # Pin tool and compile

    Labenv.env["run"] = "#{File.join(Labenv.env[PinDir],"pin")} -t #{Labenv.env[:footprint_obj]}"
    Labenv.env["prun"] = "#{File.join(Labenv.env[PinDir],"pin")} -t #{Labenv.env[:anytaskset_obj]}"
    Labenv.env["upload"] = "rsync -trv ~/.loca/data #{Labenv.env[LocaServer]}"

    # accepted = gui.verify( hash2arrlist(cmd) )
    # cmd.each{ |k,v| Labenv.env[k] = v }

    save_env

    puts @@env.inspect if Debug
    return nil
  end

  private_methods

  def self.inputs_gui( hash )
    # to build jar: javac *.java; jar cf hashTableGUI.jar *.class
    begin
      require 'src/hashTableGUI.jar'
    
      # inputs = Labenv.env.reject{ |key, _| !UserInputs.include? key }
      inputs = Hash.new
      UserInputs.each{ |k|
        inputs[k] = Labenv.env[k] || " "
      }
      input_list = hash2arrlist( inputs )
      gui = Java::hashTableGUI.new
      puts inputs.inspect if $Debug 
      input_list = gui.getInputs( input_list )
      arrlist_update_hash( input_list, hash )
      return hash
    rescue Exception # LoadError
      inputs_cmdln( hash )
      return hash
    end
  end

  def self.inputs_cmdln( hash )
    UserInputs.each do |q|
      puts "Enter the #{q} (default #{hash[q]}): "
      input = $stdin.gets.chomp
      hash[q] = input if input != ""
    end
    return hash
  end

  def self.setup_paths
    home = ENV[ ["HOME", "HOMEPATH"].detect {|h| ENV[h] != nil} ]
    raise "Could not find home directory" unless home != nil
    root = home + "/#{LocaDir}"
    if not File.directory? root
      Dir.mkdir( root, 0755 )
      warn("Locality Lab: Directory created at #{root}")
    end
    depot = root + "/#{DataDir}"
    if not File.directory? depot
      Dir.mkdir( depot, 0755 )
      warn("Locality Lab: Directory created at #{depot}")
    end
    @@paths = Hash.new
    @@paths[LocaDir] = root
    @@paths[ConfigFile] = root + "/" + ConfigFile
    @@paths[DataDir] = root + "/" + DataDir

    puts "paths = #{@@paths.inspect}" if Debug

    return @@paths
  end

  def self.save_env
    File.open(Labenv.paths[ConfigFile], 'w', 0600) do |file|
      file.puts Labenv.env.to_yaml
    end
  end

  def self.read_config
    config = Labenv.paths[ConfigFile]
    if File.exists?(config)
      @@env = YAML::load( File.read(config) )
    end
    return @@env
  end # def

  def self.hash2arrlist( hash )
    alist = Java::java.util.ArrayList.new
    hash.each{ |k,v|
      alist.add( Java::myEntry.new( k, v ))
    }
    return alist
  end

  def self.arrlist_update_hash( alist, hash )
    (0...alist.size).each{ |i|
      hash[alist.get(i).getKey] = alist.get(i).getValue
    }
    return hash
  end
end
