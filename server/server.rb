module Server

	## A method that returns all of the files
	#  and directories from the current directory
	#  which end in ".dat" or ".info".
	def self.browse(currentDir)
		files = Array.new
		dirs = Array.new
		Dir.foreach( currentDir ) do |item|
			fullname = File.join(currentDir, item)
			# Only show unhidden".dat" ".fp", and ".info" files and directories.
			if File.extname(item) == ".dat" \
			or File.extname(item) == ".dat.a" \
			or File.extname(item) == ".dat.i" \
			or File.extname(item) == ".dat.d" \
			or File.extname(item) == ".fp" \
			or File.extname(item) == ".gif" \
			or File.extname(item) == ".info" \
			or File.directory?(fullname) \
                        and item[0,1] != "." \
			and item[0,4] != "zoom"
				files << item if File.file?(fullname)
				dirs << item if File.directory?(fullname)
			end
		end

		return files, dirs
	end

	## Outputs a string-type block of html which makes the beginning of an
	#  html file.  Don't trust text coloring in gedit.
	def self.headMatter (currentDir)
		return <<Q
<!DOCTYPE html PUBLIC>
<head>
<meta name="robots" content="index, nofollow" />
<link rel="stylesheet" href="/.static/style-paper.css" type="text/css" />

<title>/loca</title>

</head>
	<body>
		<!-- Display current directroy -->
		<h2>#{currentDir}</h2>
		<!-- Display list of files and directories -->
		<table class="bigtable">

			<tr>
			  <th class="name">Name</th>
			</tr>

			<!-- Go up one directory -->
			<tr class="fileline parity0">
				<td class="name">
					<a href="#{File.join("/", File.dirname(currentDir))}">[up]</a>
				</td>
			</tr>
Q
	end

	## Outputs a string-type block of html which makes a table element
	#  with a link to the file "fileName".  Don't trust text coloring
	#  in gedit.
	def self.fileDisplayBlock(currentDir, fileName, parity )
		result = <<Q
			<tr class="fileline parity#{parity}">
				<td class="name">
					<a href="#{File.join("/", currentDir, fileName)}">
						<img src="/.static/coal-file.png" alt="dir."/> #{fileName}
					</a>
				</td>
			</tr>
Q
		return result
	end

	## Outputs a string-type block of html which makes a table element
	#  with a link to the folder "folderName".  Don't trust text coloring
	#  in gedit.
	def self.dirDisplayBlock(currentDir, dirName, parity )

		result = <<Q
			<tr class="fileline parity#{parity}">
				<td class="name">
					<a href="#{File.join("/", currentDir, dirName)}">
						<img src="/.static/coal-folder.png" alt="dir."/> #{dirName}
					</a>
				</td>
			</tr>
Q
		return result
	end

	## Outputs a string-type block of html which makes a table in the
	#  html file with links to each file and folder in the input arrays.
	#  Don't trust text coloring in gedit.
	def self.tableSection(currentDir, fileArray, dirArray)
		result = ""
		parity = 1

		fileArray.each do |file|
			result += fileDisplayBlock(currentDir, file, parity)
			parity = parity == 0 ? 1 : 0
		end

		dirArray.each do |dir|
			result += dirDisplayBlock(currentDir, dir, parity)
			parity = parity == 0 ? 1 : 0
		end

		return result
	end

	## Outputs a string-type block of html which makes the end of an
	#  html file.  Don't trust text coloring in gedit.
	def self.footMatter ()
		return <<Q
		</table>

	</body>
</html>
Q
	end

	def self.rangeForm (program, metric)
		zero = (0.0).to_s
		infinity = (eval("1e300")).to_s
		return <<Q
  <form method="post">

	&nbsp &nbsp &nbsp &nbsp &nbsp &nbsp &nbsp &nbsp &nbsp &nbsp &nbsp &nbsp
    x min: <input type="text" name="xmin" maxlength = 15
			style="text-align:right;width:80px;padding-right:4px" 
			type="text"/> &nbsp &nbsp &nbsp &nbsp
    x max: <input type="text" name="xmax" maxlength = 15
            style="text-align:right;width:80px;padding-right:4px" 
            type="text"/>

	<input type="hidden" name="program" value="#{program}">
	<input type="hidden" name="metric" value="#{metric}">
	<input type="hidden" name="whichForm" value="rangeForm">
	<input type="submit" value="Submit" />

  </form>

  <form method="POST">

	&nbsp &nbsp &nbsp &nbsp &nbsp &nbsp &nbsp &nbsp &nbsp &nbsp &nbsp &nbsp
	<input type="hidden" name="xmin" value="#{zero}">
	<input type="hidden" name="xmax" value="#{infinity}">

	<input type="hidden" name="program" value="#{program}">
	<input type="hidden" name="metric" value="#{metric}">
	<input type="hidden" name="whichForm" value="rangeForm">
	<input type="submit" value="Default" />

  </form>
Q
	end

	def self.imagePage (currentDir, program, metric,isp)
		longMetric = case metric
			when "fp" then "Average Footprint"
			when "lf" then "Data Lifetime in Cache"
			when "mr" then "Miss Ratio Curve"
			when "rd" then "Reuse Distance Distribution"
		end
    if isp
      return <<Q
<!DOCTYPE html PUBLIC>
<head>
<title>Data Plot</title>
</head>
	<body>
		<h1 align="center">#{program} - #{longMetric}</h1>
		<table align="center">
		<tr>

		<td><form method="POST">
		<button type="submit" name="goToMetric" value="fp">Average Footprint</button>
		<input type="hidden" name="whichForm" value="typeForm">
		</form></td>

		<td><form method="POST">
		<button type="submit" name="goToMetric" value="lf" disabled>Data Lifetime in Cache</button>
		<input type="hidden" name="whichForm" value="typeForm">
		</form></td>

		<td><form method="POST">
		<button type="submit" name="goToMetric" value="mr" disabled>Miss Ratio Curve</button>
		<input type="hidden" name="whichForm" value="typeForm">
		</form></td>

		<td><form method="POST">
		<button type="submit" name="goToMetric" value="rd" disabled>Reuse Distance Distribution</button>
		<input type="hidden" name="whichForm" value="typeForm">
		</form></td>

		</tr>

		</table>

		<img src="zoom_#{program}.#{metric}.png" />
		<img src="#{program}.#{metric}.png" />
		<br>
		#{rangeForm("#{program}", "#{metric}")}
	</body>
Q
    else
      return <<Q
<!DOCTYPE html PUBLIC>
<head>
<title>Data Plot</title>
</head>
	<body>
		<h1 align="center">#{program} - #{longMetric}</h1>
		<table align="center">
		<tr>

		<td><form method="POST">
		<button type="submit" name="goToMetric" value="fp">Average Footprint</button>
		<input type="hidden" name="whichForm" value="typeForm">
		</form></td>

		<td><form method="POST">
		<button type="submit" name="goToMetric" value="lf">Data Lifetime in Cache</button>
		<input type="hidden" name="whichForm" value="typeForm">
		</form></td>

		<td><form method="POST">
		<button type="submit" name="goToMetric" value="mr">Miss Ratio Curve</button>
		<input type="hidden" name="whichForm" value="typeForm">
		</form></td>

		<td><form method="POST">
		<button type="submit" name="goToMetric" value="rd">Reuse Distance Distribution</button>
		<input type="hidden" name="whichForm" value="typeForm">
		</form></td>

		</tr>

		</table>

		<img src="zoom_#{program}.#{metric}.png" />
		<img src="#{program}.#{metric}.png" />
		<br>
		#{rangeForm("#{program}", "#{metric}")}
	</body>
Q
    end
	end

end