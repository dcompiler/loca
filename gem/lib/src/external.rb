# Support for adding external tools such as Pin

require 'os'

module LabExternal
  PinTools = "source/tools/"
  FootprintCode = "src/Footprint"
  FootprintTool = "Footprint"
  #AnytasksetCode = "src/Anytaskset"
  #AnytasksetTool = "Anytaskset"

  def self.temp
    Labenv.env[Labenv::PinDir] = '/localdisk/cding/pin/pin-2.9-39599-gcc.3.4.6-ia32_intel64-linux/'
  end

  def self.add_footprint_analyzer
    require 'fileutils'
    tooldir = File.join(Labenv.env[Labenv::PinDir], PinTools)
    analyzer = File.join(LocaCmdDir, FootprintCode)
    # copy the files over
    Dir.chdir( tooldir )
    begin
      FileUtils.cp_r(analyzer, tooldir)
      Dir.chdir( FootprintTool )
      run_cmd 'make'
      raise "Fail to build the footprint analyzer" unless $?.success?
    rescue Exception => e
      msg = "Copying/making footprint analyzer directory failed"
      puts msg
      msg += e.backtrace.map{|k| k.to_s + "\n"}.to_s
      puts "Type ok to continue"
      is_ok = $stdin.gets.chomp
      raise msg unless is_ok.downcase == 'ok'
      Dir.chdir( FootprintTool )
    end
    obj_dir = Dir.entries(".").find {|i| i.match /^obj/}
    raise "Object file directory not found" if obj_dir == nil

    if OS.mac?
      obj = File.join( tooldir, FootprintTool, obj_dir, "dual_fp_all.dylib" ) #.dylib for mac, .so for windows
    else
      obj = File.join( tooldir, FootprintTool, obj_dir, "dual_fp_all.so" )
    end
    #obj = File.join( tooldir, FootprintTool, obj_dir, "linear_fp.dylib" )
    raise "Object file #{obj} not found" unless File.file?(obj)
    Labenv.env[:footprint_obj] = obj
    Labenv.save_env
  end

  # following code is for experimental anytaskset
=begin
  def self.add_anytaskset_analyzer
    require 'fileutils'
    tooldir = File.join(Labenv.env[Labenv::PinDir], PinTools)
    analyzer = File.join(LocaCmdDir, AnytasksetCode)
    # copy the files over
    Dir.chdir( tooldir )
    begin
      FileUtils.cp_r(analyzer, tooldir)
      Dir.chdir( AnytasksetTool)
      run_cmd 'make'
      raise "Fail to build the anytaskset analyzer" unless $?.success?
    rescue Exception => e
      msg = "Copying/making anytaskset analyzer directory failed"
      puts msg
      msg += e.backtrace.map{|k| k.to_s + "\n"}.to_s
      puts "Type ok to continue"
      is_ok = $stdin.gets.chomp
      raise msg unless is_ok.downcase == 'ok'
      Dir.chdir( AnytasksetTool )
    end
    obj_dir = Dir.entries(".").find {|i| i.match /^obj/}
    raise "Object file directory not found" if obj_dir == nil
    obj = File.join( tooldir, AnytasksetTool, obj_dir, "anytaskset-fp.so" )
    raise "Object file #{obj} not found" unless File.file?(obj)
    Labenv.env[:anytaskset_obj] = obj
    Labenv.save_env
  end
=end

  def self.remove_footprint_analyzer
    require 'fileutils'
    pin = File.join( Labenv.env[Labenv::PinDir], PinTools, FootprintTool)
    if not File.directory?(pin)
      warn "no footprint tool in #{pin}"
      return
    end
    FileUtils.rm_r pin
  end
end

